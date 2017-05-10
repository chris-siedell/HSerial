//
//  HSerialPort.hpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialPort_hpp
#define HSerialPort_hpp

#include <string>
#include <vector>
#include <memory>


namespace hserial {

    class HSerialController;
    class HSerialPortsManager;
    class HSerialDevice;

    /*!
     \brief A lightweight representation of a serial port.
     
     An `HSerialPort` instance represents a specific serial port. A serial port is uniquely
     identified by its device name (e.g. `"COM3"` or `"/dev/ttyS0"`),
     so instances with the same device name represent the same device. Note that
     just because an `HSerialPort` instance exists does not mean that the port is available or even
     exists -- the constructor will accept any non-empty string.
     
     An `HSerialPort` instance merely represents a port. Actually using the port is done with
     a serial port controller (a subclass of `HSerialController`). See HSerial for a
     straightforward implementation of a serial port controller.
     */

    class HSerialPort {

        friend class HSerialController;
        friend class HSerialPortsManager;

    public:

        /*!
         \brief There is no default constructor since instances are permanently associated with a specific device.
         */
        HSerialPort() = delete;

        /*!
         \brief Creates a representation of the serial port for the given device name.

         This constructor succeeds for any non-empty `deviceName`, regardless if the given serial
         port is valid or present.
         
         \params deviceName The string identifying the serial device. For example `"COM3"` or `"/dev/ttyS0"`.

         \throws std::invalid_argument if `deviceName` is empty.
         */
        HSerialPort(const std::string& deviceName);

        /*!
         \brief Returns the name of the serial device represented by the HSerialPort instance.
         
         The device name is the same thing as the port property of a `Serial` object. The %HSerial
         library differs from the Serial library in that the device name cannot be changed after
         an object is created.

         \returns The device name of the port.
         */
        std::string getDeviceName() const;

        /*!
         \brief Returns a description of the port.
         \returns The description of the port. May be an empty string.
         */
        std::string getDescription();

        /*!
         \brief Returns the hardware ID of the port.
         \returns The hardware ID of the port. May be an empty string.
         */
        std::string getHardwareID();

        /*!
         \brief Determines if the instances represent the same port.
         */
        bool operator==(const HSerialPort& other);

        /*!
         \brief Determines if the instances represent different ports.
         */
        bool operator!=(const HSerialPort& other);

        /*!
         \brief Returns a pointer to the controller which currently controls the serial port.

         Technical details: The current controller is not necessarily the controller that is actually
         using the serial
         port. This is because a controller can use another controller though delegation (see the
         [implementation notes](@ref delegation)). The current controller is the controller
         that is ultimately responsible for delegating to the controller which is -- or was most
         recently -- using the serial port.

         __Warning__: This is a potentially dangerous function in a multithreaded environment. The
         current controller might change or be destroyed from another thread at any time.
         Calling code must be cautious in how it uses the pointer.

         \returns A pointer to the current controller. May be `NULL`.
         */
        HSerialController* getCurrentController();

    private:

        /*!
         \brief This device constructor is used by HSerialPortsManager.
         */
        HSerialPort(std::shared_ptr<HSerialDevice> device);

        /*!
         \brief HSerialPort instances share ownership of the device object with HSerialPortsManager.
         */
        std::shared_ptr<HSerialDevice> device;
    };
    
}


#endif /* HSerialPort_hpp */
