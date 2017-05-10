//
//  HSerialDevice.hpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialDevice_hpp
#define HSerialDevice_hpp

#include <string>
#include <memory>
#include <mutex>

#include "HSerialAccess.hpp"


/// \cond internal_docs

namespace hserial {

    class HSerialController;
    class HSerialPortsManager;
    class HSerialPort;

    /*!
     \brief An internal object uniquely representing a serial port device.
     
     There is only one HSerialDevice instance per device name.

     HSerialPort instances are effectively proxies for instances of this class. Ownership
     of a device object is shared between the associated HSerialPort instances and the
     HSerialPortsManager singleton instance.
     */

    class HSerialDevice {

        friend class HSerialController;
        friend class HSerialPortsManager;
        friend class HSerialPort;
        
    public:

        HSerialDevice() = delete;
        HSerialDevice(const HSerialDevice&) = delete;
        void operator=(const HSerialDevice&) = delete;
        HSerialDevice(HSerialDevice&&) = delete;
        void operator=(HSerialDevice&&) = delete;

    private:


#pragma mark - Friends Stuff

        /*!
         \name Friends Stuff

         These are the methods and attributes intended for use by friend classes
         (HSerialController, HSerialPortsManager, and HSerialPort).
         */
        ///@{

        /*!
         \brief The device name of the serial port.
         
         This string never changes.
         
         For use by friend classes.
         */
        const std::string deviceName;

        /*!
         \brief Returns a description of the serial device.
        
         For use by friend classes.
         */
        std::string getDescription();

        /*!
         \brief Returns the hardware ID of the serial device.
         
         For use by friend classes.
         */
        std::string getHardwareID();

        /*!
         \brief Updates the description and hardwareID strings.

         For use by friend classes.
         */
        void setDescriptionAndHardwareID(const std::string& description, const std::string& hardwareID);

        /*!
         \brief Returns a pointer to the current controller.
         
         For use by friend classes.
         */
        HSerialController* getCurrentController();

        /*!
         \brief Creates a HSerialDevice object.

         HSerialPortsManager determines when an HSerialDevice needs to be created, and this is the
         function it calls to do it.
         
         The `description` and `hardwareID` strings may be empty.
         
         For use by friend classes.
         */
        static std::shared_ptr<HSerialDevice> createShared(const std::string& deviceName, const std::string& description, const std::string& hardwareID);

        /*!
         \brief Returns the shared access object for the device, creating it if necessary.
         
         For use by friend classes.
        */
        std::shared_ptr<HSerialAccess> getAccess();

        ///@} /Friends


#pragma mark - Internal Stuff

        /*!
         \name Internal Stuff
         
         These are the methods and attributes meant for the device's internal use. Friend classes
         should leave these alone.
         */
        ///@{

        // Documented in implementation.
        struct MakeSharedEnabler;

        /*!
         \brief [Internal] Called from `createShared`.
         
         HSerialDevice instances are meant to be created only with the createShared() method.
         
         Internal use only.

         \sa createShared()
         */
        HSerialDevice(const std::string& deviceName, const std::string& description, const std::string& hardwareID);

        ~HSerialDevice();

        /*!
         \brief [Internal] The access object associated with the serial device.
         
         HSerialController must use getAccess() to obtain its copy of the access object.
        
         The access is lazily instantiated in getAccess(). HSerialController objects associated with
         a given device share ownership of the access using `shared_ptr`s. The device uses a
         `weak_ptr` so that the access can be created, shared, and destroyed as needed.
         
         Internal use only.
         */
        std::weak_ptr<HSerialAccess> access;

        /*!
         \brief [Internal] A string describing the serial port.

         Internal use only.

         \sa getDescription(), setDescriptionAndHardwareID()
         */
        std::string description;

        /*!
         \brief [Internal] A string describing the serial port.
         
         Internal use only.

         \sa getHardwareID(), setDescriptionAndHardwareID()
         */
        std::string hardwareID;

        /*!
         \brief [Internal] Protects `description` and `hardwareID`.

         `detailsMutex` protects `description` and `hardwareID` reading and writing since they may
         change at any time. (They may be empty strings initially that are updated in later.)
         
         Internal use only.
         */
        std::mutex detailsMutex;

        ///@} /Internal
    };
    
}

/// \endcond internal_doc

#endif /* HSerialDevice_hpp */
