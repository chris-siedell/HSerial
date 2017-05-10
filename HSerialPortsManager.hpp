//
//  HSerialPortsManager.hpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialPortsManager_hpp
#define HSerialPortsManager_hpp

#include <unordered_map>
#include <vector>
#include <mutex>

#include <serial/serial.h>

#include "HSerialPort.hpp"


namespace hserial {


    class HSerialDevice;

    /*!
     \brief A singleton class for discovering and monitoring serial ports.

     To obtain the manager call HSerialPortsManager::getInstance().

     Currently HSerialPortsManager is just a fancy wrapper for `serial::list_ports`,
     but there are plans to develop it further.
     */
    class HSerialPortsManager {

        friend class HSerialPort;

    public:

        /*!
         \brief Returns the single instance of the serial ports manager.
         \returns The singleton instance of the serial ports manager.
         */
        static HSerialPortsManager& getInstance();

        /*!
         \brief Returns a list of serial ports currently found on the system.
         \returns A list of serial ports currently found on the system.
         */
        std::vector<HSerialPort> getPorts();

        /*!
         \brief Returns a serial port for the given device name. 
         
         This is similar to using the device name constructor of HSerialPort. Note that this
         function will return an HSerialPort instance for any non-empty `deviceName`, even for
         devices that are invalid or not present.
         
         \returns A serial port for the given device name.

         \throws std::invalid_argument if `deviceName` is empty.
         */
        HSerialPort portForDeviceName(const std::string& deviceName);

        HSerialPortsManager(const HSerialPortsManager&) = delete;
        HSerialPortsManager& operator=(const HSerialPortsManager&) = delete;
        HSerialPortsManager(HSerialPortsManager&&) = delete;
        HSerialPortsManager& operator=(HSerialPortsManager&&) = delete;

    private:

        /*!
         \brief Returns a shared pointer to the device object for the given device name.
         
         This method will create the device object if it does not already exist.
         */
        std::shared_ptr<HSerialDevice> getDevice(const std::string& deviceName);

        /*!
         \brief Updates the device's information, if possible.
         
         Specifically, this method updates `description` and `hardwareID`. If it is not possible
         to update those properties this method does nothing.
         */
        void refreshDeviceDetails(HSerialDevice& device);

        HSerialPortsManager() {}
        ~HSerialPortsManager() {}

        std::shared_ptr<HSerialDevice> getDeviceInternal(const std::string& deviceName);
        std::shared_ptr<HSerialDevice> getDeviceInternal(const serial::PortInfo& portInfo);

        /*!
         \brief Used to protect the devices dictionary, as well as to serialize calls
         to `serial::list_ports`.
         */
        std::mutex devicesMutex;

        /*!
         \brief A dictionary of devices indexed by their names.
         
         The manager instance shares ownership of a device with any HSerialPort
         instances.
         */
        std::unordered_map<std::string, std::shared_ptr<HSerialDevice>> devices;

    };
}
#endif /* HSerialPortsManager_hpp */
