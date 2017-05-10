//
//  HSerial.cpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerial.hpp"

#include "HSerialExceptions.hpp"


namespace hserial {

    // todo: comments in some of the functions may be out of date -- review and consolidate
    //  into documentation

#pragma mark - AccessManagementGuard

    /*!
     \brief Used internally by HSerial's state changing access management functions.
     
     This scope based object is used to do the following:
     - It serializes the access management function calls using am_serializingMutex, so these
     functions never run concurrently.
     - It sets the am_callInProgress and am_callThread properties. These are used by the
     willMakeInactive callback (via transitionInitiatedExternally()) to determine if the change
     was initiated by this controller or another controller.
    */
    class HSerial::AccessManagementGuard {
    public:
        AccessManagementGuard(HSerial& _owner) : owner(_owner), serializingLock(_owner.am_serializingMutex) {
            std::lock_guard<std::mutex> lock(owner.am_mutex);
            owner.am_callInProgress = true;
            owner.am_callThread = std::this_thread::get_id();
        }
        ~AccessManagementGuard() {
            std::lock_guard<std::mutex> lock(owner.am_mutex);
            owner.am_callInProgress = false;
        }
    private:
        std::lock_guard<std::mutex> serializingLock;
        HSerial& owner;
    };


#pragma mark - Construction/Destruction

    HSerial::HSerial(const std::string& deviceName) : HSerialController(deviceName) {}

    HSerial::HSerial(HSerialPort port) : HSerialController(port) {}

    HSerial::~HSerial() {
        try {
            removeFromAccess();
        } catch (...) {
            close();
            removeFromAccess();
        }
    }


#pragma mark - Public

    std::string HSerial::getControllerType() const {
        return "HSerial";
    }


#pragma mark - Access Functions

    void HSerial::setSettings(uint32_t baudrate, serial::Timeout timeout, serial::bytesize_t bytesize, serial::parity_t parity,
                     serial::stopbits_t stopbits, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent) {
        HSerialController::setSettings(baudrate, timeout, bytesize, parity, stopbits, flowcontrol, onlyIfDifferent);
    }


#pragma mark - Access Management

    bool HSerial::isActive() const {
        return HSerialController::isActive();
    }

    bool HSerial::isLockedActive() const {
        // See the documentation for _isLockedActive for an explanation of why it is used instead
        //  of isLocked.
        std::lock_guard<std::mutex> lock(isLockedMutex);
        return _isLockedActive;
    }

    void HSerial::makeActive() {
        // todo: review these comments for up-to-dateness and transfer to documentation
        //  ---------------------------------------------------------------------------------------
        // | Before            | After              | Analysis
        // |-------------------|--------------------|----------------------------------------------
        // | locked active     | locked active      | makeActive returns without doing anything.
        // |                   | no exception       |  isLocked and _isLockedActive remain true
        // |                   |                    |  since didMakeActive is never called.
        // |-------------------|--------------------|----------------------------------------------
        // | active (unlocked) | active (unlocked)  | makeActive returns without doing anything.
        // |                   | no exception       |  isLocked and _isLockedActive remain false
        // |                   |                    |  since didMakeActive is never called.
        // |-------------------|--------------------|----------------------------------------------
        // | inactive          | active (unlocked)  | makeActive makes the controller active -- if
        // |                   | possible           |  it throws, it does so after the transition
        // |                   |  SettingsError     |  (it will be SettingsError). isLocked and
        // |                   |  exception         |  _isLockedActive were false before (since
        // |                   |                    |  inactive) and will redundantly be set to
        // |                   |                    |  false in didMakeActive (before SettingsError
        // |                   |                    |  can be thrown).
        // |-------------------|--------------------|----------------------------------------------
        // | inactive          | inactive           | makeActive is called but throws before the
        // |                   | exception thrown   |  transition (probably with ControllerRefuses).
        // |                   |                    |  The controller remains inactive, and isLocked
        // |                   |                    |  _isLockedActive remain false since
        // |                   |                    |  didMakeActive is not called.
        //  ---------------------------------------------------------------------------------------
        AccessManagementGuard guard(*this);
        setIsLockedActive = false;
        HSerialController::makeActive();
    }

    void HSerial::makeInactive() {
        // todo: review these comments for up-to-dateness and transfer to documentation

        // Note: the "before" states are considered at the point the makeInactive request is finally
        //  processed by the HSerialAccess code. Suppose this controller is locked active when
        //  called and there is a simultaneous request by another controller to make this
        //  controller inactive. If that other request is processed before the makeInactive request
        //  below then the other controller's request is too early and will be declined.
        //  ---------------------------------------------------------------------------------------
        // | Before            | After              | Analysis
        // |-------------------|--------------------|----------------------------------------------
        // | locked active     | inactive           | makeInactive is called. The willMakeInactive
        // |                   | no exception       |  callback detects that the request was
        // |                   |                    |  initiated by this controller and ignores the
        // |                   |                    |  isLocked property. The transition occurs and
        // |                   |                    |  the didMakeInactive callback sets isLocked
        // |                   |                    |  to false.
        // |                   |                    | No exceptions may be thrown after the
        // |                   |                    |  transition since they would have to come from
        // |                   |                    |  the new active controller's didMakeActive
        // |                   |                    |  callback, but there is no new active
        // |                   |                    |  controller.
        // |-------------------|--------------------|----------------------------------------------
        // | active (unlocked) | inactive           | Same as above, except setting isLocked to
        // |                   | no exception       |  false is redundant.
        // |-------------------|--------------------|----------------------------------------------
        // | inactive          | inactive           | makeInactive returns without doing anything.
        // |                   | no exception       |  isLocked remains false.
        // |-------------------|--------------------|----------------------------------------------
        // | locked active     | locked active      | makeInactive is called but throws before the
        // |                   | exception thrown   |  transition. (The exception should be
        // |                   |                    |  ControllerRefuses.) Since didMakeInactive is
        // |                   |                    |  not called isLocked remains true.
        // |-------------------|--------------------|----------------------------------------------
        // | active (unlocked) | active (unlocked)  | Same as above, except isLocked remains false.
        // |                   | exception thrown   |
        //  ---------------------------------------------------------------------------------------

        // Why not tentatively set isLocked to false before calling makeInactive, similarly to how
        //  isLocked is set to true before the call to makeActive in makeLockedActive?
        // The cases are not symmetrical. In makeLockedActive we know there is no concurrent
        //  code that can make the controller active (since HSerial::makeActive and
        //  HSerial::makeLockedActive are the only public functions that can make the controller
        //  active, and they are serialized by AccessManagementGuard). This is not true for making
        //  a controller inactive. Any controller on any thread can request this controller to
        //  become inactive at any time (the requests are processed one at a time by HSerialAccess,
        //  but they can be made at any time). So suppose the call to makeInactive fails. Then we
        //  need to restore the value of isLocked before rethrowing the exception. However, between
        //  those two steps the controller might become inactive due to a request from another
        //  thread. Avoiding or detecting such situations would be messy, and that messiness is not
        //  necessary.

        AccessManagementGuard guard(*this);
        HSerialController::makeInactive();
    }

    void HSerial::makeLockedActive() {

        // See the documentation for extensive analysis of this code's logic.

        // Initial
        AccessManagementGuard guard(*this);

        // Pre-Request
        setIsLockedActive = true;
        {
            std::lock_guard<std::mutex> lock(isLockedMutex);
            isLocked = true;
        }

        try {
            // Make Active Request
            HSerialController::makeActive();
        } catch (...) {
            // Request Failure
            if (!HSerialController::isActive()) {
                std::lock_guard<std::mutex> lock(isLockedMutex);
                isLocked = false;
            }
            throw;
        }

        // Post-Request
        {
            std::lock_guard<std::mutex> lock(isLockedMutex);
            _isLockedActive = true;
        }
    }

    void HSerial::unlockActive() {
        // todo: review these comments for up-to-dateness and transfer to documentation
        //  ---------------------------------------------------------------------------------------
        // | Before            | After              | Analysis
        // |-------------------|--------------------|----------------------------------------------
        // | locked active     | active (unlocked)  | isLocked is set to false.
        // |                   | no exception       |
        // |-------------------|--------------------|----------------------------------------------
        // | active (unlocked) | active (unlocked)  | isLocked is redundantly set to false.
        // |                   | no exception       |
        // |-------------------|--------------------|----------------------------------------------
        // | inactive          | inactive           | isLocked is redundantly set to false.
        // |                   | no exception       |
        //  ---------------------------------------------------------------------------------------
        AccessManagementGuard guard(*this);
        std::lock_guard<std::mutex> lock(isLockedMutex);
        isLocked = false;
        _isLockedActive = false;
    }

    void HSerial::removeFromAccess() {
        // todo: review these comments for up-to-dateness and transfer to documentation
        // The analysis is the same as for makeInactive. The only differences in behavior would
        //  occur if this controller was being used as a delegate or if a derived class uses
        //  delegates, but we can't do anything about those situations here.
        // This function is intended for use in the destructor.
        AccessManagementGuard guard(*this);
        HSerialController::removeFromAccess();
    }


#pragma mark - Transition Callbacks

    void HSerial::willMakeInactive() {

        // First, decide whether the transition is allowed. This depends on why this callback is
        //  being called. There are two ways to get here:
        //  - (1) Another controller requested to become active, requiring that this controller
        //        become inactive. This is an externally initiated transition.
        //  - (2) The controller itself requested to become inactive with a call to
        //        makeInactive or removeFromAccess. This is an internally initiated transition.
        // In case 1 we need to check if the controller is locked. If so, the transition is not
        //  allowed and must be cancelled by throwing ControllerRefuses.
        // In case 2 we ignore the isLocked property. Essentially, in this case the controller
        //  is automatically unlocked (but only if the transition occurs).
        if (transitionInitiatedExternally()) {
            std::lock_guard<std::mutex> lock(isLockedMutex);
            if (isLocked) {
                throw ControllerRefuses(*this, "The controller is locked.");
            }
            // else: Transition is allowed.
        }
        // else: Transition is allowed (even if locked) since self initiated.
        //       Unlock on success (in didMakeInactive).

        // Fulfill the callback's obligations to block access calls and ensure that they have all
        //  returned. The following is adapted from HSerialController's default implementation.
        blockAccessCalls();
        bool success = waitForAllAccessCallsToReturn(std::chrono::milliseconds(1000));
        if (!success) {
            std::stringstream ss;
            ss << "The controller is using the port. Controller: " << this->getDescription() << ".";
            throw ControllerRefuses(*this, ss.str());
        }

        // We require isLockedMutex to be locked during the transition to keep isLocked and
        //  _isLockedActive in sync with the active state.
        // The mutex is unlocked in didMakeInactive or didCancelMakeInactive, one of which
        //  is guaranteed to be called.
        isLockedMutex.lock();
    }

    void HSerial::didMakeInactive() noexcept {
        // Design Guarantee: isLocked and _isLockedActive must always be false when the contoller
        //  is inactive.
        // isLockedMutex was locked in willMakeInactive.
        isLocked = false;
        _isLockedActive = false;
        isLockedMutex.unlock();
    }

    void HSerial::didCancelMakeInactive() noexcept {
        // This callback is included for completeness. However it should never be called since
        //  the willMakeInactive callback blocks access calls and waits for all access calls to
        //  return before returning.
        // In this case isLocked and _isLockedActive are not changed.
        // isLockedMutex was locked in willMakeInactive.
        isLockedMutex.unlock();
    }

    void HSerial::willMakeActive() noexcept {
        // Keep the isLockedMutex locked throughout the transition so that _isLockedActive and
        //  isLocked are never out of sync with the active state if transitioning from inactive
        //  to locked active.
        // The mutex is unlocked in didMakeActive.
        isLockedMutex.lock();
    }

    void HSerial::didMakeActive() {
        // Design Guarantee: isLocked and _isLockedActive must always be set appropriately
        //  by this callback. The value set depends on which function, makeActive or
        //  makeLockedActive, initiated the transition.
        // isLockedMutex was locked in willMakeActive.
        // Analysis: HSerial::makeActive and HSerial::makeLockedActive are the only public functions
        //  that can cause didMakeActive to be called. Since these functions cannot run concurrently
        //  (due to the AccessManagementGuard) the setIsLockedActive flag is thread safe -- the
        //  value set by the two public functions (false for makeActive and true for
        //  makeLockedActive) cannot change unexpectedly.
        isLocked = setIsLockedActive;
        _isLockedActive = setIsLockedActive;
        isLockedMutex.unlock();
    }


#pragma mark - Miscellaneous

    bool HSerial::transitionInitiatedExternally() {
        std::lock_guard<std::mutex> lock(am_mutex);
        return !am_callInProgress || am_callThread != std::this_thread::get_id();
    }

}
