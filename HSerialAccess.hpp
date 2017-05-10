//
//  HSerialAccess.hpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialAccess_hpp
#define HSerialAccess_hpp

#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>

#include <serial/serial.h>


namespace hserial {

    class HSerialPort;
    class HSerialController;
    class HSerialDevice;

    /*!
     \brief An internal object that controls access to the serial port.
     
     HSerialController instances rely on the access object to interact with the serial port.

     An access object acts as a proxy for a `serial::Serial` object. It is
     created as needed by the HSerialDevice instance. If no objects need to use the serial port
     (i.e. there are no controllers) then there will be no access object.

     The access object also acts as a gatekeeper. It allows only one controller at a time (the 
     active controller) to use the underlying Serial object.
     
     Access objects are owned by HSerialController instances. HSerialDevice keeps a weak reference
     to its access object, and it is from the device object that the controller gets
     its access object.
    */
    class HSerialAccess {

        friend class HSerialController; // uses the access
        friend class HSerialDevice; // creates the access as needed

    public:

        HSerialAccess() = delete;
        HSerialAccess(const HSerialAccess&) = delete;
        void operator=(const HSerialAccess&) = delete;
        HSerialAccess(HSerialAccess&&) = delete;
        void operator=(HSerialAccess&&) = delete;

    private:


#pragma mark - Creating an Access Object

        /*!
         \name Creating an Access Object
         */
        /// \{

        /*!
         \brief Creates an access object and returns it as a shared pointer..

         HSerialDevice calls this function when it determines that an access object needs to be
         created.

         Note: HSerialController obtains its access object by calling HSerialDevice::getAccess().

         \see HSerialDevice::getAccess()
         */
        static std::shared_ptr<HSerialAccess> createShared(const std::string& deviceName);
        
        /// \} /Creating an Access Object


#pragma mark - Controller Access Management

        /*!
         \name Controller Access Management

         These are the backing functions for %HSerialController's
         [access management functions](\ref controller-access-management).
         */
        /// \{

        bool isActive(const HSerialController& controller) const;
        void makeActive(HSerialController& controller);
        void makeInactive(HSerialController& controller);
        void removeFromAccess(HSerialController& controller);

        /// \} /Controller Access Management
        

#pragma mark - Controller Access Functions

        /*!
         \name Controller Access Functions

         These are the backing functions for %HSerialController's
         [access functions](\ref controller-access-functions).
         */
        /// \{

        void open(const HSerialController& controller);
        void ensureOpen(const HSerialController& controller);
        bool isOpen(const HSerialController& controller) const;
        void close(const HSerialController& controller);
        size_t available(const HSerialController& controller);
        bool waitReadable(const HSerialController& controller);
        void waitByteTimes(const HSerialController& controller, size_t count);
        size_t read(const HSerialController& controller, uint8_t* buffer, size_t size);
        size_t read(const HSerialController& controller, std::vector<uint8_t>& buffer, size_t size);
        size_t read(const HSerialController& controller, std::string& buffer, size_t size);
        std::string read(const HSerialController& controller, size_t size);
        size_t readline(const HSerialController& controller, std::string& buffer, size_t size, std::string eol);
        std::string readline(const HSerialController& controller, size_t size, std::string eol);
        std::vector<std::string> readlines(const HSerialController& controller, size_t size, std::string eol);
        size_t write(const HSerialController& controller, const uint8_t* data, size_t size);
        size_t write(const HSerialController& controller, const std::vector<uint8_t>& data);
        size_t write(const HSerialController& controller, const std::string &data);
        void setBaudrate(const HSerialController& controller, uint32_t baudrate, bool onlyIfDifferent);
        uint32_t getBaudrate(const HSerialController& controller) const;
        void setTimeout(const HSerialController& controller, serial::Timeout& timeout, bool onlyIfDifferent);
        serial::Timeout getTimeout(const HSerialController& controller) const;
        void setBytesize(const HSerialController& controller, serial::bytesize_t bytesize, bool onlyIfDifferent);
        serial::bytesize_t getBytesize(const HSerialController& controller) const;
        void setParity(const HSerialController& controller, serial::parity_t parity, bool onlyIfDifferent);
        serial::parity_t getParity(const HSerialController& controller) const;
        void setStopbits(const HSerialController& controller, serial::stopbits_t stopbits, bool onlyIfDifferent);
        serial::stopbits_t getStopbits(const HSerialController& controller) const;
        void setFlowcontrol(const HSerialController& controller, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent);
        serial::flowcontrol_t getFlowcontrol(const HSerialController& controller) const;
        void setSettings(const HSerialController& controller, uint32_t baudrate, serial::Timeout timeout, serial::bytesize_t bytesize,
                         serial::parity_t parity, serial::stopbits_t stopbits, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent);
        void flush(const HSerialController& controller);
        void flushInput(const HSerialController& controller);
        void flushOutput(const HSerialController& controller);
        void sendBreak(const HSerialController& controller, int duration);
        void setBreak(const HSerialController& controller, bool level);
        void setRTS(const HSerialController& controller, bool level);
        void setDTR(const HSerialController& controller, bool level);
        bool waitForChange(const HSerialController& controller);
        bool getCTS(const HSerialController& controller);
        bool getDSR(const HSerialController& controller);
        bool getRI(const HSerialController& controller);
        bool getCD(const HSerialController& controller);


        /// \} /Controller Access Functions


#pragma mark - Controller Transition Utilities

        /*!
         \name Controller Transition Utilities
         
         These are the backing functions for %HSerialController's
         [transition utilities](\ref controller-transition-utilities).
         */
        /// \{

        void blockAccessCalls(const HSerialController& controller);
        void unblockAccessCalls(const HSerialController& controller);
        bool waitForAllAccessCallsToReturn(const HSerialController& controller, const std::chrono::milliseconds& timeout);

        /// \} /Controller Transition Utilities


#pragma mark - Internal Condition Variables and Mutexes

        /*!
         \brief [Internal] Used to alert waiting threads that all access calls have returned.
         
         Declared mutable to support the const controller access functions.
         
         Used in waitForAllAccessCallsToReturn.
         
         state_numUnreturnedAccessCalls is used for the predicate. Uses stateMutex.
         
         Internal use only.
         
         \see waitForAllAccessCallsToReturn
         */
        mutable std::condition_variable allAccessCallsReturnedCondition;

        /*!
         \brief [Internal] Used to block access calls.

         The predicate of this condition uses:
         - state_transitionInProgress,
         - state_transitionThread, and
         - state_accessIsUnblocked.

         Uses stateMutex.

         Notifications are made from unblockAccessCalls. the AccessUnblocker destructor,
         and the TransitionBlocker destructor. Usually a AccessUnblocker object is destroyed
         immediately before a TransitionBlocker object. The one exception occurs when an active
         controller change is concurrent with a current controller change. This happens when the
         active controller change is initiated from the willRemove or didCancelRemove callbacks.

         It is waited on in the AccessGuard constructor.

         Declared mutable to support the const controller access functions.

         Internal use only.
         */
        mutable std::condition_variable accessUnblockedCondition;

        /*!
         \brief [Internal] This mutex is used to serialize access calls to some of the Serial functions.

         See the [design notes](\ref multithreading-serial) for an explanation.

         Declared mutable to support the const controller access functions.

         Internal use only.
         */
        mutable std::mutex accessSerializingMutex;

        /*!
         \brief [Internal] Used for several properties and condition variables.
         
         See the [internal state properties](\ref internal-state-properties) for all the variables
         protected by this mutex. Additionally, the allAccessCallsReturnedCondition and the
         accessUnblockedCondition condition variables use this mutex.

         Declared mutable to support the const controller access functions.
         
         Internal use only.
         
         \see allAccessCallsReturnedCondition, accessUnblockedCondition
         */
        mutable std::mutex stateMutex;


#pragma mark - State Variables

        /*!
         \name [Internal] State Variables

         The `state_` variables describe the state of the access object. In most cases these
         should not be read or written unless stateMutex is locked.
         */
        /// \{

        /*!
         \brief [Internal] The current controller.

         currentController is atomic so that other threads can read the instantaneous value
         without having to use a mutex.

         HSerialDevice reads this property directly in HSerialDevice::getCurrentController().
         */
        std::atomic<HSerialController*> state_currentController {NULL};

        /*!
         \brief [Internal] The active controller.

         activeController is atomic so that other threads can read the instantaneous value
         without having to use a mutex.
         */
        std::atomic<HSerialController*> state_activeController {NULL};

        /*!
         \brief [Internal] This is the number of unreturned access calls.

         Used as the predicate of allAccessCallsReturnedCondition. Protected by stateMutex.

         Declared mutable to support the const controller access functions. This is reasonable
         since any access function will both increment and decrement this counter -- the net
         effect is always zero.

         Internal use only.

         \see allAccessCallsReturnedCondition
         */
        mutable int state_numUnreturnedAccessCalls = 0;

        /*!
         \brief [Internal] Indicates when access is unblocked.

         This property forms part of the predicate of accessUnblockedCondition. Protected by
         stateMutex.
         
         Access blocking only occurs during a transition. Even then, it doesn't apply to calls made
         on the transition thread.

         Internal use only.

         \see accessUnblockedCondition
         */
        bool state_accessIsUnblocked = true;

        /*!
         \brief [Internal] Indicates if a transition is in progress.

         This property is changed only by TransitionBlocker objects.
         
         Internal use only.
         */
        bool state_transitionInProgress;

        /*!
         \brief [Internal] Indicates if a concurrent active controller change is allowed.

         Most controller changes cannot be performed concurrently. There is an exception for an
         active controller change initiated from the willRemove and didCancelRemove callbacks
         (i.e. during a current controller change).
         This flag is used to specify when such a concurrent change is allowed.

         This property is meaningful only when transitionInProgress is `true`.

         This property is made atomic to avoid multiple lockings and unlockings of stateMutex in
         performCurrentControllerChange.
         
         Internal use only.
         */
        std::atomic_bool state_concurrentActiveControllerChangeAllowed;

        /*!
         \brief [Internal] Identifies the transition thread.

         This property is meaningful only when transitionInProgress is `true`.
         
         Internal use only.
         */
        std::thread::id state_transitionThread;
        
        /// \} /State Variables


#pragma mark - TransitionBlocker Variables

        /*!
         \name TransitionBlocker Variables
         
         These variables are for TransitionBlocker instances used by the access object.
         */
        /// \{

        /*!
         \brief [Internal] Coordinates using `tb_`* properties of the access.
         
         For use by TransitionBlocker instances only.
         */
        std::mutex tb_mutex;

        /*!
         \brief [Internal] Indicates the number of the transition that may proceed.
         
         When a TransitionBlocker's assigned number (see tb_nextNumber) equals the ready number
         then the transition may proceed.
         
         Forms the predicate of tb_readyCondition.
         
         For use by TransitionBlocker instances only.
         */
        uint32_t tb_readyNumber = 0;

        /*!
         \brief [Internal] The next available transition number.
         
         A TransitionBlocker instance begins by assigning itself a number.
         
         For use by TransitionBlocker instances only.
         */
        uint32_t tb_nextNumber = 0;

        /*!
         \brief [Internal] Indicates when the ready number changes.
         
         tb_readyNumber is used in the predicate.
         
         For use by TransitionBlocker instances only.
         */
        std::condition_variable tb_readyCondition;

        /// \} /TransitionBlocker Variables


#pragma mark - Internal Access Management Functions

        /*!
         \name Internal Access Management Functions
         
         Functions used to implement the access management functions.
         */
        /// \{

        /*!
         \brief [Internal] Indicates if the given controller is in the access list.

         The access list consists of the current controller and its delegates (of all degrees).

         This function assumes that the current controller and its delegates cannot change during
         the call. The first requirement can be met by calling with stateMutex locked or in a
         context where currentController cannot be changed by any other thread. The second
         requirement is met by requiring a controller to register its delegates before making itself
         active (i.e. in the constructor).

         If the current controller is NULL this function returns false.
         
         Internal use only.
         */
        bool isInAccessList(const HSerialController& controller);

        /*!
         \brief Used to determine if an active controller change should be performed immediately.

         Most controller changes (active and current) cannot be performed concurrently and must be
         queued and serialized (using a TransitionBlocker object).
         The only exception is for active controller changes initiated from a willRemove or
         didCancelRemove callback.
         Such a change runs concurrently with the current controller change that is responsible for
         the callback being called.
         (There can be only one such concurrent active controller change since it has to be
         performed on the callback thread.)
         This function determines if a controller change is being requested in such circumstances.

         This function locks stateMutex to evaluate its expression. Note that the return result
         remains valid even after the mutex is unlocked, unless the calling thread explictly does
         something to change its value. Proof of this claim is given in the comments of the
         implementation.

         Internal use only.
         
         \see makeActive, makeInactive
         */
        bool shouldPerformConcurrentActiveControllerChange(const HSerialController& controller);

        /*!
         \brief [Internal] Used to set the active controller.

         This is one of two functions used internally to perform controller changes.
         performCurrentControllerChange is the other function.

         This function makes newActiveController the active controller (it may be `NULL`) without
         changing the access list. Therefore a non-NULL newActiveController must be on the access
         list. This assumption is not checked -- the calling code must guarantee it.

         This function may be called concurrently with a current controller change only if
         shouldPerformConcurrentActiveControllerChange returns true. Otherwise the change must be
         queued and serialized with a TransitionBlocker object.

         Internal use only.

         \see performCurrentControllerChange, shouldPerformConcurrentActiveControllerChange,
         performTransition
         */
        void performActiveControllerChange(HSerialController* newActiveController);

        /*!
         \brief [Internal] Used to set the current controller.

         This is one of two functions used internally to perform controller changes.
         performActiveControllerChange is the other function.

         This function changes the current controller and makes the new current controller active.
         Accepts NULL.

         This function should never be called concurrently. This requirement is guaranteed by using
         TransitionBlocker objects.
         
         Internal use only.

         \see performActiveControllerChange, performTransition
         */
        void performCurrentControllerChange(HSerialController* newCurrentController);

        /*!
         \brief [Internal] Calls the old active controller's willMakeInactive and sets the new active
         controller.

         This helper function is used by both performActiveControllerChange and
         performCurrentControllerChange to perform common work.
         This function
         - calls the willMakeInactive callback for the old active controller, and
         - sets the new active controller after verifying that it is safe to do so. This means that
         access calls are blocked, and that there are no unreturned access calls.

         It is assumed that
         - newActiveController is different than the old active controller,
         - an AccessUnblocker instance is in place in the calling context, and
         - the calling function will call the new active controller's didMakeActive.

         Internal use only.

         \see performActiveControllerChange, performCurrentControllerChange
         */
        void performTransition(HSerialController* newController, bool alsoSetAsCurrentController);

        /// \} /Internal Access Management Functions


#pragma mark - Other Internal Stuff

        /*!
         \name Other Internal Stuff
         */
        /// \{

        /*!
         \brief [Internal] Throws NotActiveController if the controller is not active.

         Declared const to support the const controller access functions.

         Assumes stateMutex is locked.

         Internal use only.
         */
        void throwIfNotActiveController(const HSerialController& controller, const char* funcName) const;

        /*!
         \brief [Internal] Throws logic_error if not called during a transition and on the transition thread.

         This function verifies that:
         - the function is being called during a transition,
         - and that the call is made on the transition thread.

         This is used by the controller access blocking functions: blockAccessCalls(),
         unblockAccessCalls(), and waitForAllAccessCallsToReturn().

         Assumes stateMutex is locked.

         Internal use only.
         */
        void throwIfNotTransitionCorrect(const HSerialController& controller, const char* funcName);

        /*!
         \brief [Internal] Called by createShared().

         Internal use only.

         \see createShared(), HSerialDevice::getAccess()
         */
        HSerialAccess(const std::string& deviceName);

        ~HSerialAccess();

        /*!
         \brief [Internal] The serial::Serial object for actually using the port.
         
         Internal use only.
         */
        serial::Serial serial;

        /// \} /Other Internal Stuff


#pragma mark - Internal Classes

        // These are documented in the implementation.
        struct MakeSharedEnabler; // supports createShared()
        class AccessGuard; // used to control and monitor access calls
        class AccessUnblocker; // used to automatically unblock access calls after a transition
        class TransitionBlocker; // used to queue and serialize controller changes

    };
}

#endif /* HSerialAccess_hpp */
