//
//  HSerial.hpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerial_hpp
#define HSerial_hpp

#include <atomic>
#include <mutex>
#include <thread>
#include <serial/serial.h>


#include "HSerialController.hpp"

namespace hserial {

    // todo: Consider making it easier for other controllers to use the same locking mechanism
    //       employed here. Options:
    //          - make an HSerialLockableController abstract base case which this controller
    //            derives from, or
    //          - put the functionality directly into HSerialController.

    
    /*!
     \brief A minimal serial controller that mimics the interface of `serial::Serial`.
     
     `HSerial` is a subclass of `HSerialController` that mimics the interface of `serial::Serial`.
     For the most part it can be used just like a `Serial` object, but it must be made "active" 
     before it can use the port (`NotActiveController` exceptions will be thrown otherwise). Being
     active means the controller has exclusive access to the serial port.
     */

    class HSerial : public HSerialController {

    public:

        /*!
         Creates an %HSerial serial port controller.
         
         An %HSerial serial port controller mimics the %serial::Serial interface for using
         a serial port.
         
         \param deviceName A string identifying the serial device to use, such as `"COM3"` or
         `"/dev/ttyS0"`.
         \see HSerial(HSerialPort port);
         */
        HSerial(const std::string& deviceName);

        /*!
         Creates an %HSerial serial port controller.

         An %HSerial serial port controller mimics the %serial::Serial interface for using
         a serial port.

         \param port The port to use.
         \see HSerial(const std::string& deviceName);
         */
        HSerial(HSerialPort port);

        virtual ~HSerial();

        /*!
         \brief Returns `"HSerial"`.
         \returns The string `"HSerial"`.
         */
        virtual std::string getControllerType() const;


#pragma mark - Access Management

        /*!
         \name Access Management

         These functions manage the controller's access to the serial port. When the
         controller is active it has exclusive access to the port. It will remain active until
         it gives up the active role deliberately or until another controller requests to
         take over the active role. Locking the controller prevents other controllers from
         taking over the active role.
         */
        /// \{

        /*!
         \brief Indicates if the controller is active.

         When active the controller has exclusive access to the serial port.

         Warning: In a multithreaded environment the controller may become
         inactive at any time, including between when this function returns `true` and when your
         code uses that value.

         \see isLockedActive
         */
        bool isActive() const;

        /*!
         \brief Indicates if the controller is locked active.

         Being locked active means that the controller is active and it is locked into that state.
         It will not relinquish the
         active role until makeInactive() or unlockActive() are called.
         
         \see makeInactive, unlockActive
        */
        bool isLockedActive() const;

        /*!
         \brief Makes the controller active if it is not already.

         Beware that in a multithreaded environment the controller may become inactive at
         any time unless it is locked. Consider makeLockedActive().
         
         If the controller was already locked active then this function does nothing and
         it remains locked active.
         
         Calling code should always catch ControllerRefuses.

         \throws hserial::ControllerRefuses Thrown if the current controller (or any controller
         delegated to by the current controller) refuses to give up the active role. In this case
         the controller is not active.
         \see makeLockedActive
         */
        void makeActive();

        /*!
         \brief Makes the controller inactive if it is not already.

         This function will also unlock the controller if necessary.

         If this function throws an exception then the controller's state is unchanged. If it was
         locked active when called it will remain locked active. Automatic unlocking occurs only if
         the inactivation request is successful.
         
         Calling code should always catch ControllerRefuses.

         \throws hserial::ControllerRefuses Thrown if the controller is still using the port. For
         example, if the controller has a call to one of the reading, writing, or waiting functions
         that hasn't returned yet.
         \see makeLockedActive, unlockActive
         */
        void makeInactive();

        /*!
         \brief Makes the contoller locked active if not already.

         If this function returns normally then the controller is locked active, meaning it is
         active and it will refuse all requests to relinquish the active role. 
         It will remain locked active until unlockActive or makeInactive are called.

         If the controller was already locked active then this function does nothing.

         Calling code should always catch ControllerRefuses.

         \throws hserial::ControllerRefuses Thrown if the current controller (or any controller
         delegated to by the current controller) refuses to give up the active role. In this case
         the controller is not active.
         \see unlockActive, makeInactive
         */
        void makeLockedActive();

        /*!
         \brief Unlocks the controller if locked.

         After unlocking the controller may become inactive at any time.
         
         If the controller was not locked active then this function does nothing.

         \see makeInactive, makeLockedActive
         */
        void unlockActive();
        
        /// \} /Access Management


#pragma mark - Access Functions

        /*!
         \name Opening and Closing the Port

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::open;
        using HSerialController::ensureOpen;
        using HSerialController::isOpen;
        using HSerialController::close;

        /// \} /Opening and Closing the Port


        /*!
         \name Setting the Port's Properties

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::setBaudrate;
        using HSerialController::getBaudrate;
        using HSerialController::setTimeout;
        using HSerialController::getTimeout;
        using HSerialController::setBytesize;
        using HSerialController::getBytesize;
        using HSerialController::setParity;
        using HSerialController::getParity;
        using HSerialController::setStopbits;
        using HSerialController::getStopbits;
        using HSerialController::setFlowcontrol;
        using HSerialController::getFlowcontrol;

        /*! \copydoc HSerialController:setSettings() */
        void setSettings(uint32_t baudrate = 9600,
                         serial::Timeout timeout = serial::Timeout::simpleTimeout(500),
                         serial::bytesize_t bytesize = serial::eightbits,
                         serial::parity_t parity = serial::parity_none,
                         serial::stopbits_t stopbits = serial::stopbits_one,
                         serial::flowcontrol_t flowcontrol = serial::flowcontrol_none,
                         bool onlyIfDifferent = false);

        /// \} /Setting the Port's Properties
        

        /*!
         \name Reading from the Port

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::read;
        using HSerialController::readline;
        using HSerialController::readlines;

        /// \} /Reading from the Port


        /*!
         \name Writing to the Port

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::write;

        /// \} /Writing to the Port


        /*!
         \name Waiting on Reads and Writes

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::available;
        using HSerialController::waitReadable;
        using HSerialController::waitByteTimes;

        /// \} /Waiting on Reads and Writes


        /*!
         \name Flushing the Buffers

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::flush;
        using HSerialController::flushInput;
        using HSerialController::flushOutput;

        /// \} /Flushing the Buffers


        /*!
         \name Working with the Control Lines

         These access functions may throw NotActiveController.
         */
        /// \{

        using HSerialController::sendBreak;
        using HSerialController::setBreak;
        using HSerialController::setRTS;
        using HSerialController::setDTR;
        using HSerialController::waitForChange;
        using HSerialController::getCTS;
        using HSerialController::getDSR;
        using HSerialController::getRI;
        using HSerialController::getCD;

        /// \} /Working with the Control Lines


    protected:

#pragma mark - Internal: Transition Callbacks

        /*!
         \name Internal: Transition Callbacks
         */
        /// \{
        virtual void willMakeInactive();
        virtual void didMakeInactive() noexcept;
        virtual void didCancelMakeInactive() noexcept;
        virtual void willMakeActive() noexcept;
        virtual void didMakeActive();
        /// \} /Internal: Transition Callbacks


#pragma mark - Internal: AccessManagementGuard Stuff

        /*!
         \name Internal: AccessManagementGuard Stuff
         
         The `"am_"` variables are used by AccessManagementGuard instances.
         */
        /// \{

        /*!
         \brief Serializes calls to the state changing access management functions.
        */
        std::mutex am_serializingMutex;

        /*!
         \brief Used to protect am_callInProgress and am_callThread.
         */
        std::mutex am_mutex;

        /*!
         \brief Used to help identify when willMakeInactive is being called due to an external
         controller.
         
         am_mutex must be locked when reading and writing this variable.
         
         \see transitionInitiatedExternally
         */
        bool am_callInProgress = false;

        /*!
         \brief Used to help identify when willMakeInactive is being called due to an external
         controller.
         
         This variable is meaningful only if am_callInProgress is `true`.
         
         am_mutex must be locked when reading and writing this variable.
         
         \see transitionInitiatedExternally
         */
        std::thread::id am_callThread;

        /// \} /Internal: AccessManagementGuard Stuff


#pragma mark - Internal: Miscellaneous Stuff

        /*!
         \name Internal: Miscellaneous Stuff
         */
        /// \{

        /*!
         \brief Protects isLocked and _isLockedActive.
         
         Declared mutable to support isLockedActive() const-ness.
         */
        mutable std::mutex isLockedMutex;

        /*!
         \brief Determines if the controller may become inactive.
         
         The willMakeInactive callback uses this variable to decide if the transition is allowed.

         It is ignored if the transition was initiated internally (e.g. by a call to makeInactive).
         In these cases the variable is set to false in the didMakeInactive callback. Essentially,
         the controller is automatically unlocked when the transition is initiated by the
         controller itself.
         
         isLockedMutex must be locked to read or write this variable.

         \see _isLockedActive, isLockedActive
         */
        bool isLocked;

        /*!
         \brief The backing variable for isLockedActive().

         The isLocked variable is the one actually used by willMakeInactive to determine whether
         to allow the transition. The _isLockedActive variable is used only for returning a value
         from isLockedActive().

         Most of the time isLocked and _isLockedActive will have identical values. However, during
         makeLockedActive isLocked is tentatively and temporarily set to true even if the
         controller is not active. _isLockedActive remains unchanged until it is certain that the
         controller is locked and active.

         Using two variables -- the functional isLocked, and the informational _isLockedActive --
         is one solution for overcoming the fact that isLocked may be inconsistent with the active
         state during makeLockedActive. The other solution would be to block isLockedActive()
         during a makeLockedActive() call. However, makeLockedActive can take a significant
         amount of time to execute. Using the _isLockedActive variable allows the isLockedActive
         function to return promptly.
         
         For more information refer to
         [the makeLockedActive analysis](\ref makeLockedActive-analysis] in the design notes.

         isLockedMutex must be locked to read or write this variable.

         \see isLocked, isLockedActive
         */
        bool _isLockedActive = false;

        /*!
         \brief Indicates the value to give isLocked and _isLockedActive after making the
         controller active.
        
         This flag is set in both of the public functions that can make the controller active
         (`false` in makeActive and `true` in makeLockedActive). It is applied in the didMakeActive
         callback.
         
         By setting isLocked and _isLockedActive in the didMakeActive callback (with isLockedMutex
         continuously locked during the transition) there is never a moment when the controller is
         active but unlocked when transitioning from inactive to locked active.
         */
        bool setIsLockedActive = false;

        /*!
         \brief Determines if the transition responsible for a callback being called was initiated
         by another controller.
         
         This function is used in the willMakeInactive callback to decide if the isLocked
         variable should be checked.

         This function may be used only in a transition callback. It depends on properties set by
         AccessManagementGuard.
         */
        bool transitionInitiatedExternally();

        /*!
         \brief This access management function is used during destruction.
         */
        void removeFromAccess();

        /// \} /Internal: Miscellaneous Stuff

        
        // AccessManagementGuard is documented in the implementation file.
        class AccessManagementGuard;

    };

}

#endif /* HSerial_hpp */
