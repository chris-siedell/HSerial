//
//  HSerialAccess.cpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerialAccess.hpp"

#include <algorithm>
#include <cassert>


#include "HSerialController.hpp"


namespace hserial {

    // HSerialAccess::setTimeout uses this operator.
    bool operator!=(const serial::Timeout& A, const serial::Timeout& B) {
        if (A.inter_byte_timeout != B.inter_byte_timeout) return true;
        if (A.read_timeout_constant != B.read_timeout_constant) return true;
        if (A.read_timeout_multiplier != B.read_timeout_multiplier) return true;
        if (A.write_timeout_constant != B.write_timeout_constant) return true;
        if (A.write_timeout_multiplier != B.write_timeout_multiplier) return true;
        return false;
    }

#pragma mark - TransitionBlocker

    /*!
     \brief Queues and serializes controller changes by blocking threads.

     Only one TransitionBlocker instance exists at a time (others are blocked in the constructor).
     This prevents concurrent controller changes unless this mechanism is explicitly bypassed, as
     occurs for active controller changes initiated from willRemove and didCancelRemove callbacks
     (see shouldPerformConcurrentActiveControllerChange()).

     The TransitionBlocker object is also responsible for setting the following 
     HSerialAccess state variables:
     - state_transitionInProgress
     - state_concurrentActiveControllerChangeAllowed (set to false)
     - state_transitionThread
     
     \see shouldPerformConcurrentActiveControllerChange
     */
    class HSerialAccess::TransitionBlocker {
    public:
        /*!
         \brief Blocks until the thread may perform a transition.
         
         Transitions are queued and serialized by using a TransitionBlocker object. Only one
         TransitionBlocker object exists at a time -- any others are blocked in their constructors.
         */
        TransitionBlocker(HSerialAccess& _access) : access(_access) {
            std::unique_lock<std::mutex> lock(access.tb_mutex);
            uint32_t number = access.tb_nextNumber++;
            auto predicate = [number, this]() {
                return number == access.tb_readyNumber;
            };
            access.tb_readyCondition.wait(lock, predicate);
            lock.unlock();

            // Initiate transition.
            std::lock_guard<std::mutex> stateLock(access.stateMutex);
            access.state_transitionInProgress = true;
            access.state_concurrentActiveControllerChangeAllowed.store(false);
            access.state_transitionThread = std::this_thread::get_id();
        }
        /*!
         \brief Ends the transition, allowing the next queued transition.
         */
        ~TransitionBlocker() {

            // Terminate transition.
            {
                std::lock_guard<std::mutex> stateLock(access.stateMutex);
                access.state_transitionInProgress = false;
            }
            access.accessUnblockedCondition.notify_all();

            std::unique_lock<std::mutex> lock(access.tb_mutex);
            access.tb_readyNumber += 1;
            lock.unlock();
            access.tb_readyCondition.notify_all();
        }
    private:
        HSerialAccess& access;
    };


#pragma mark - AccessUnblocker

    /*!
     \brief Insures that access calls are unblocked after a transition.

     Access calls are blocked during a controller transition. This scoped-based object ensures that
     they are unblocked after the transition, whether it is completed normally or is interrupted
     by an exception.

     This object is used in performCurrentControllerChange() and performActiveControllerChange(),
     the two functions responsible for transitions.

     Most transitions are not concurrent. The only exception is an active controller change
     initiated from the willRemove or didCancelRemove callbacks during a current controller change.
     So in this situation access is automatically unblocked after each concurrent active controller
     change.

     Internal use only.

     \see performCurrentControllerChange, performActiveControllerChange
     */
    class HSerialAccess::AccessUnblocker {
    public:
        AccessUnblocker(HSerialAccess& _access) : access(_access) {}
        /*!
         \brief Unblocks access calls if they are blocked.
         */
        ~AccessUnblocker() {
            std::unique_lock<std::mutex> lock(access.stateMutex);
            if (!access.state_accessIsUnblocked) {
                access.state_accessIsUnblocked = true;
                lock.unlock();
                access.accessUnblockedCondition.notify_all();
            }
        }
    private:
        HSerialAccess& access;
    };


#pragma mark - AccessGuard

    /*!
     \brief Monitors and controls access calls.

     The AccessGuard is a scoped based object used with the access functions. It has the following
     responsibilities:

     - It blocks the access call, if required.
     - It ensures that the controller making the call has permission to do so (i.e. it is the active
     controller).
     - It increments and decrements the unreturned access calls counter, which allows code on other
     threads to know if the controller is using the serial port.
     - If the access call was the last access call to return it broadcasts that fact.

     The AccessGuard allows concurrent access calls. Serializing access calls requires a
     separate mechanism (see `accessSerializingMutex`).

     When there isn't a transition being performed access calls are always unblocked.

     During a transition access calls may be blocked on threads other than the transition thread.
     */
    class HSerialAccess::AccessGuard {
    public:
        /*!
         \brief Prepares for an access call.
         
         Specifically, this function:
         - blocks the thread, if required,
         - checks that the controller is the active controller, and
         - increments the counter of unreturned access calls.
         */
        AccessGuard(const HSerialAccess& _access, const HSerialController& controller, const char* funcName) : access(_access) {
            auto predicate = [this]() {
                // Returns true when the given access call may proceed (is unblocked).
                if (access.state_transitionInProgress) {
                    // Access calls may be blocked during a transition.
                    if (std::this_thread::get_id() == access.state_transitionThread) {
                        // However, access calls are never blocked on the transition thread.
                        return true;
                    } else {
                        return access.state_accessIsUnblocked;
                    }
                } else {
                    // Access calls are unblocked on all threads if there isn't a transition.
                    return true;
                }
            };
            std::unique_lock<std::mutex> lock(access.stateMutex);
            access.accessUnblockedCondition.wait(lock, predicate);
            access.throwIfNotActiveController(controller, funcName);
            access.state_numUnreturnedAccessCalls += 1;
        }
        /*!
         \brief Officially ends an access call.
         
         This function:
         - decrements the counter of unreturned access calls, and
         - notifies waiting threads if all access calls have returned.
         */
        ~AccessGuard() {
            std::unique_lock<std::mutex> lock(access.stateMutex);
            int n = --access.state_numUnreturnedAccessCalls;
            lock.unlock();
            if (n == 0) {
                // It is OK if numUnreturnedAccessCalls is incremented after unlocking -- all the
                //  condition variable signifies is that the number reached zero at some point.
                //  Guaranteeing that the number of access calls stays at zero requires call
                //  blocking.
                // Consider: this condition is only waited on in waitForAllAccessCallsToReturn,
                //  which is only allowed to execute during a transition. Should this notification
                //  only occur during transitions? (It should be harmless as is.)
                access.allAccessCallsReturnedCondition.notify_all();
            }
        }
    private:
        const HSerialAccess& access;
    };


#pragma mark - Controller Access Management

    bool HSerialAccess::isActive(const HSerialController& controller) const {
        // state_activeController is atomic for this function. Users are warned in the
        //  documentation that the value may change at any time.
        return &controller == state_activeController.load();
    }

    void HSerialAccess::makeActive(HSerialController& controller) {
        if (shouldPerformConcurrentActiveControllerChange(controller)) {
            // There can't be more than one thread at a time meeting this condition since
            //  shouldPerform* returns true only when called on the transition thread during
            //  a current controller transition. Therefore the active controller won't change
            //  even though stateMutex is unlocked.
            if (&controller != state_activeController.load()) {
                performActiveControllerChange(&controller);
            }
        } else {
            // Need to perform a queued and serialized transition.
            TransitionBlocker blocker(*this);

            // After blocker's constructor is finished the thread has exclusive control of
            //  controller changes for the lifetime of the blocker object.
            // Note that the blocker object sets several of the necessary state flags and variables.
            // A concurrent active controller change is possible only if the
            //  state_concurrentActiveControllerChangeAllowed flag is set to true (it is initially
            //  false). This occurs in performCurrentControllerChange.

            // Determine the type of controller change required. This has to be done after waiting
            //  in the queue since the access list or active controller may have changed.
            // The access list won't change during the call to isInAccessList due to the blocker
            //  object.
            if (isInAccessList(controller)) {
                // If the controller is in the access list then perform an active controller change,
                //  but only if not redundant.
                if (&controller != state_activeController.load()) {
                    performActiveControllerChange(&controller);
                }
            } else {
                // The controller is not in the access list so a current controller change is
                //  required to make it active. This case include the access list being empty --
                //  i.e. the current controller being NULL.
                performCurrentControllerChange(&controller);
            }
        }
    }

    void HSerialAccess::makeInactive(HSerialController& controller) {
        // If this controller is the active controller then it sets the active controller to NULL.
        //  Otherwise it does nothing. This function does not change the access list -- it never
        //  requires a current controller change.
        // Many comments in makeActive apply here.
        if (shouldPerformConcurrentActiveControllerChange(controller)) {
            if (&controller == state_activeController.load()) {
                performActiveControllerChange(NULL);
            }
        } else {
            TransitionBlocker blocker(*this);
            if (&controller == state_activeController.load()) {
                performActiveControllerChange(NULL);
            }
        }
    }

    void HSerialAccess::removeFromAccess(HSerialController& controller) {
        // Removing the controller from the access requires a current controller change, which
        //  must always be queued.
        TransitionBlocker blocker(*this);
        // The blocker object makes the call to isInAccessList safe.
        if (isInAccessList(controller)) {
            if (&controller == state_currentController.load()) {
                performCurrentControllerChange(NULL);
            } else {
                // removeFromAccess is primarily intended to be called from a controller's
                //  destructor. If a controller is being destroyed while it is a delegate (i.e. in
                //  the access list but not the current controller) then its delegating controller
                //  is incorrectly implemented. Delegates must remain valid for the lifetime of the
                //  delegating controller.
                std::stringstream ss;
                ss << "Cannot remove controller from access if it is not the current controller. Controller: "
                << controller.getDescription() << ".";
                throw std::logic_error(ss.str());
            }
        }
    }


#pragma mark - Controller Access Functions

    void HSerialAccess::open(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.open();
    }

    void HSerialAccess::ensureOpen(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!serial.isOpen()) {
            serial.open();
        }
    }

    bool HSerialAccess::isOpen(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.isOpen();
    }

    void HSerialAccess::close(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.close();
    }

    size_t HSerialAccess::available(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.available();
    }

    bool HSerialAccess::waitReadable(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        // Waiting functions are not serialized.
        return serial.waitReadable();
    }

    void HSerialAccess::waitByteTimes(const HSerialController& controller, size_t count) {
        AccessGuard guard(*this, controller, __func__);
        // Waiting functions are not serialized.
        serial.waitByteTimes(count);
    }

    size_t HSerialAccess::read(const HSerialController& controller, uint8_t* buffer, size_t size) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.read(buffer, size);
    }

    size_t HSerialAccess::read(const HSerialController& controller, std::vector<uint8_t>& buffer, size_t size) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.read(buffer, size);
    }

    size_t HSerialAccess::read(const HSerialController& controller, std::string& buffer, size_t size) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.read(buffer, size);
    }

    std::string HSerialAccess::read(const HSerialController& controller, size_t size) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.read(size);
    }

    size_t HSerialAccess::readline(const HSerialController& controller, std::string& buffer, size_t size, std::string eol) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.readline(buffer, size, eol);
    }

    std::string HSerialAccess::readline(const HSerialController& controller, size_t size, std::string eol) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.readline(size, eol);
    }

    std::vector<std::string> HSerialAccess::readlines(const HSerialController& controller, size_t size, std::string eol) {
        AccessGuard guard(*this, controller, __func__);
        // Reading functions are not serialized.
        return serial.readlines(size, eol);
    }

    size_t HSerialAccess::write(const HSerialController& controller, const uint8_t* data, size_t size) {
        AccessGuard guard(*this, controller, __func__);
        // Writing functions are not serialized.
        return serial.write(data, size);
    }

    size_t HSerialAccess::write(const HSerialController& controller, const std::vector<uint8_t>& data) {
        AccessGuard guard(*this, controller, __func__);
        // Writing functions are not serialized.
        return serial.write(data);
    }

    size_t HSerialAccess::write(const HSerialController& controller, const std::string &data) {
        AccessGuard guard(*this, controller, __func__);
        // Writing functions are not serialized.
        return serial.write(data);
    }

    void HSerialAccess::setBaudrate(const HSerialController& controller, uint32_t baudrate, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || baudrate != serial.getBaudrate()) {
            serial.setBaudrate(baudrate);
        }
    }

    uint32_t HSerialAccess::getBaudrate(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getBaudrate();
    }

    void HSerialAccess::setTimeout(const HSerialController& controller, serial::Timeout& timeout, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || timeout != serial.getTimeout()) {
            serial.setTimeout(timeout);
        }
    }

    serial::Timeout HSerialAccess::getTimeout(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getTimeout();
    }

    void HSerialAccess::setBytesize(const HSerialController& controller, serial::bytesize_t bytesize, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || bytesize != serial.getBytesize()) {
            serial.setBytesize(bytesize);
        }
    }

    serial::bytesize_t HSerialAccess::getBytesize(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getBytesize();
    }

    void HSerialAccess::setParity(const HSerialController& controller, serial::parity_t parity, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || parity != serial.getParity()) {
            serial.setParity(parity);
        }
    }

    serial::parity_t HSerialAccess::getParity(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getParity();
    }

    void HSerialAccess::setStopbits(const HSerialController& controller, serial::stopbits_t stopbits, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || stopbits != serial.getStopbits()) {
            serial.setStopbits(stopbits);
        }
    }

    serial::stopbits_t HSerialAccess::getStopbits(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getStopbits();
    }

    void HSerialAccess::setFlowcontrol(const HSerialController& controller, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        if (!onlyIfDifferent || flowcontrol != serial.getFlowcontrol()) {
            serial.setFlowcontrol(flowcontrol);
        }
    }

    serial::flowcontrol_t HSerialAccess::getFlowcontrol(const HSerialController& controller) const {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getFlowcontrol();
    }

    void HSerialAccess::setSettings(const HSerialController& controller, uint32_t baudrate, serial::Timeout timeout, serial::bytesize_t bytesize,
                                    serial::parity_t parity, serial::stopbits_t stopbits, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);

        if (!onlyIfDifferent || baudrate != serial.getBaudrate()) {
            serial.setBaudrate(baudrate);
        }

        if (!onlyIfDifferent || timeout != serial.getTimeout()) {
            serial.setTimeout(timeout);
        }

        if (!onlyIfDifferent || bytesize != serial.getBytesize()) {
            serial.setBytesize(bytesize);
        }

        if (!onlyIfDifferent || parity != serial.getParity()) {
            serial.setParity(parity);
        }

        if (!onlyIfDifferent || stopbits != serial.getStopbits()) {
            serial.setStopbits(stopbits);
        }

        if (!onlyIfDifferent || flowcontrol != serial.getFlowcontrol()) {
            serial.setFlowcontrol(flowcontrol);
        }
    }

    void HSerialAccess::flush(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        // Flushing functions are not serialized.
        serial.flush();
    }

    void HSerialAccess::flushInput(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        // Flushing functions are not serialized.
        serial.flushInput();
    }

    void HSerialAccess::flushOutput(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        // Flushing functions are not serialized.
        serial.flushOutput();
    }

    void HSerialAccess::sendBreak(const HSerialController& controller, int duration) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.sendBreak(duration);
    }

    void HSerialAccess::setBreak(const HSerialController& controller, bool level) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.setBreak(level);
    }

    void HSerialAccess::setRTS(const HSerialController& controller, bool level) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.setRTS(level);
    }

    void HSerialAccess::setDTR(const HSerialController& controller, bool level) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        serial.setDTR(level);
    }

    bool HSerialAccess::waitForChange(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        // Waiting functions are not serialized.
        return serial.waitForChange();
    }

    bool HSerialAccess::getCTS(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getCTS();
    }

    bool HSerialAccess::getDSR(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getDSR();
    }

    bool HSerialAccess::getRI(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getRI();
    }

    bool HSerialAccess::getCD(const HSerialController& controller) {
        AccessGuard guard(*this, controller, __func__);
        std::lock_guard<std::mutex> lock(accessSerializingMutex);
        return serial.getCD();
    }


#pragma mark - Controller Access Blocking

    void HSerialAccess::blockAccessCalls(const HSerialController& controller) {
        std::lock_guard<std::mutex> lock(stateMutex);
        throwIfNotTransitionCorrect(controller, __func__);
        throwIfNotActiveController(controller, __func__);
        state_accessIsUnblocked = false;
    }

    void HSerialAccess::unblockAccessCalls(const HSerialController& controller) {
        std::unique_lock<std::mutex> lock(stateMutex);
        throwIfNotTransitionCorrect(controller, __func__);
        throwIfNotActiveController(controller, __func__);
        state_accessIsUnblocked = true;
        lock.unlock();
        accessUnblockedCondition.notify_all();
    }

    bool HSerialAccess::waitForAllAccessCallsToReturn(const HSerialController& controller, const std::chrono::milliseconds& timeout) {
        std::unique_lock<std::mutex> lock(stateMutex);
        throwIfNotTransitionCorrect(controller, __func__);
        throwIfNotActiveController(controller, __func__);
        return allAccessCallsReturnedCondition.wait_for(lock, timeout, [this]() {return state_numUnreturnedAccessCalls == 0;});
    }


#pragma mark - Internal Stuff Used for Access Management

    bool HSerialAccess::isInAccessList(const HSerialController& controller) {
        HSerialController* cc = state_currentController.load();
        return cc != NULL && (cc == &controller || cc->hasAsDelegateOrSubdelegate(&controller));
    }

    bool HSerialAccess::shouldPerformConcurrentActiveControllerChange(const HSerialController& controller) {
        /*
         Claim: The expression's value cannot be changed by any thread other than the calling
         thread, even after stateMutex is unlocked.

         First, a definition: The transition thread is the thread on which a transition was
         initiated, that is, the thread makeActive or makeInactive was called on.
         (removeFromAccess doesn't apply since that will only perform current controller changes.)

         The proof of the claim depends on these statements which are guaranteed true by
         the implementation:

         - (1) The transition thread does not change during a transition. It is set once at the
         beginning of a transition in the TransitionBlocker object.

         - (2) The variables that are used in the expression are changed only on the transition
         thread. Namely, these dependencies are:
         - state_transitionInProgress: set to true/false by the TransitionBlocker, which
         by definition exists only on the transition thread.
         - state_concurrentActiveControllerChangeAllowed: set to false at the beginning of a
         transition in the TransitionBlocker, and then possibly changed in
         performCurrentControllerChange, which is called only on the transition thread.
         - state_transitionThread: set in the TransitionBlocker constructor.
         - state_currentController (isInAccessList): changed only in performTransition, which
         is only called only on the transition thread.
         - the controller's delegates and subdelegates tree (implicit in isInAccessList): if
         a controller is implemented correctly then its delegates tree is fixed before
         it is used.

         First consider the case where there is no transition in progress. In this case the
         expression is false. Another thread may initiate a transition but the expression remains
         false since the calling thread will not be the transition thread.

         Now consider the case where there is a transition in progress. Divide this into
         two subcases depending on whether the calling thread is the transition thread.

         If the calling thread is the transition thread then statements 1 and 2 guarantee that
         only the calling thread (i.e. the transition thread) can change the expression's value.

         If the calling thread is not the transition thread then the expression is false.
         By statement 1 and the definition of 'transition thread' the calling thread cannot become
         the transition thread except by its own action. Therefore the expression must remain false,
         regardless of the actions of other threads.
         */
        std::unique_lock<std::mutex> lock(stateMutex);
        // The order of evaluation is important since state_concurrentActiveControllerChangeAllowed
        //  and state_transitionThread are undefined if state_transitionInProgress is false.
        return state_transitionInProgress   && state_concurrentActiveControllerChangeAllowed.load()
                                            && state_transitionThread == std::this_thread::get_id()
                                            && isInAccessList(controller);
    }

    void HSerialAccess::performActiveControllerChange(HSerialController* newActiveController) {
        // Called from makeActive or makeInactive.

        AccessUnblocker unblocker(*this);

        performTransition(newActiveController, false); // calls willMakeInactive, may throw

        if (newActiveController) {
            newActiveController->didMakeActive(); // may throw
        }
    }

    void HSerialAccess::performCurrentControllerChange(HSerialController* newCurrentController) {
        // Called from makeActive or removeFromAccess.

        AccessUnblocker unblocker(*this);

        std::vector<HSerialController*> oldAccessList;
        std::vector<HSerialController*> notifiedList;

        // The active controller may change up to the point performTransition is called since
        //  concurrent active controller changes are allowed from the willRemove and
        //  didCancelRemove callbacks.

        // Note: we don't have to worry about the current controller changing with the stateMutex
        //  unlocked since performCurrentControllerChange is never called concurrently.
        HSerialController* oldCurrentController = state_currentController.load();
        if (oldCurrentController){
            oldAccessList = oldCurrentController->getControllersList();
        }

        try {

            // concurrentActiveControllerChangeAllowed must be true when the willRemove
            //  and didCancelRemove callbacks are made. It must be false during the other callbacks.
            // It is false at this point.

            state_concurrentActiveControllerChangeAllowed.store(true);

            for (HSerialController* x : oldAccessList) {
                x->willRemove(); // may throw
                notifiedList.push_back(x);
            }

            state_concurrentActiveControllerChangeAllowed.store(false);

            performTransition(newCurrentController, true); // calls willMakeInactive, may throw

        } catch (...) {
            // Setting the concurrency flag again is required if the exception was thrown from
            //  willMakeInactive.
            // Doing this is redundant, but harmless, if the exception came from willRemove.
            state_concurrentActiveControllerChangeAllowed.store(true);

            for (HSerialController* x : notifiedList) {
                x->didCancelRemove(); // noexcept
            }

            // We don't need to unset the concurrency flag since the transition is
            //  being terminated (the flag will be undefined at that point).

            throw;
        }

        for (HSerialController* x : oldAccessList) {
            x->didRemove(); // noexcept
        }

        if (newCurrentController) {

            // Get the new access list and reverse it, since didAdd is called in reverse order
            //  from the controllers list (highest degree delegates down to current controller).
            std::vector<HSerialController*> newAccessList = newCurrentController->getControllersList();
            std::reverse(newAccessList.begin(), newAccessList.end());

            for (HSerialController* x : newAccessList) {
                x->didAdd(); // noexcept
            }

            newCurrentController->didMakeActive(); // may throw
        }
    }

    void HSerialAccess::performTransition(HSerialController* newController, bool alsoSetAsCurrentController) {
        // Called from performActiveControllerChange or performCurrentControllerChange.
        // The activeController and currentController are never changed except in this function.
        // This function is never called concurrently since most controller changes are queued and
        //  serialized, and the only exception is for an active controller change initiated from
        //  the willRemove and didCancelRemove callbacks (i.e. not from here).

        HSerialController* oldActiveController = state_activeController.load();

        // A correctly implemented willMakeInactive callback will
        //  - block access calls, and
        //  - ensure all access calls have returned.
        if (oldActiveController) {
            oldActiveController->willMakeInactive(); // may throw
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            // Ensure requirements are met for a safe transition. Namely:
            //  - access calls are blocked, and
            //  - there are no unreturned access calls.
            try {

                // Access calls must be blocked during the transition.
                if (state_accessIsUnblocked) {
                    if (oldActiveController) {
                        std::stringstream ss;
                        ss << "Access calls must be blocked in willMakeInactive. Controller: "
                            << oldActiveController->getDescription() << ".";
                        throw std::logic_error(ss.str());
                    } else {
                        state_accessIsUnblocked = false;
                    }
                }

                // There must be no unreturned access calls during the transition.
                if (state_numUnreturnedAccessCalls > 0) {
                    if (oldActiveController) {
                        std::stringstream ss;
                        ss << "There are " << state_numUnreturnedAccessCalls
                            << " unreturned access calls after willMakeInactive was called. Controller: "
                            << oldActiveController->getDescription() << ".";
                        throw std::logic_error(ss.str());
                    } else {
                        // This should not happen.
                        throw std::logic_error("There are unreturned access calls for a NULL controller.");
                    }
                }

            } catch (...) {
                if (oldActiveController) {
                    oldActiveController->didCancelMakeInactive(); // noexcept
                }
                throw;
            }

            if (newController) {
                newController->willMakeActive(); // noexcept
            }

            // Perform the transition.
            state_activeController.store(newController);
            if (alsoSetAsCurrentController) {
                state_currentController.store(newController);
            }
        }

        if (oldActiveController) {
            oldActiveController->didMakeInactive(); // noexcept
        }
    }


#pragma mark - Other Internal Stuff

    void HSerialAccess::throwIfNotActiveController(const HSerialController& controller, const char* funcName) const {
        // Assumes stateMutex is locked.
        if (&controller != state_activeController.load()) {
            std::stringstream ss;
            ss << "The controller must be active to call " << (funcName ? funcName : "NULL")
                << ". Inactive controller: " << controller.getDescription() << ".";
            throw std::logic_error(ss.str());
        }
    }

    void HSerialAccess::throwIfNotTransitionCorrect(const HSerialController& controller, const char* funcName) {
        // Assumes stateMutex is locked.
        if (!state_transitionInProgress || std::this_thread::get_id() != state_transitionThread) {
            std::stringstream ss;
            ss << "Calling " << (funcName ? funcName : "NULL") << " is allowed only from a transition callback or subcall. Controller: "
                << controller.getDescription() << ".";
            throw std::logic_error(ss.str());
        }
    }


#pragma mark - shared_ptr Creation

    /*! \copydoc HSerialDevice::MakeSharedEnabler */
    struct HSerialAccess::MakeSharedEnabler : public HSerialAccess {
        MakeSharedEnabler(const std::string& deviceName) : HSerialAccess(deviceName) {}
    };

    std::shared_ptr<HSerialAccess> HSerialAccess::createShared(const std::string& deviceName) {
        return std::make_shared<MakeSharedEnabler>(deviceName);
    }


#pragma mark - Construction/Destruction

    HSerialAccess::HSerialAccess(const std::string& deviceName) {
        // Setting the port here (instead of putting the serial object in the initializer list with
        //  the deviceName as a parameter) means that the port stays closed until explicitly opened
        //  by the user.
        serial.setPort(deviceName);
    }

    HSerialAccess::~HSerialAccess() {}

}
