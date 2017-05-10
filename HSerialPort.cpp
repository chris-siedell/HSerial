//
//  HSerialPort.cpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerialPort.hpp"

#include "HSerialPortsManager.hpp"
#include "HSerialDevice.hpp"


namespace hserial {

    HSerialPort::HSerialPort(const std::string& deviceName) {
        HSerialPortsManager& manager = HSerialPortsManager::getInstance();
        device = manager.getDevice(deviceName);
    }

    HSerialPort::HSerialPort(std::shared_ptr<HSerialDevice> _device) {
        device = std::move(_device);
    }

    bool HSerialPort::operator==(const HSerialPort& other) {
        return device == other.device;
    }

    bool HSerialPort::operator!=(const HSerialPort& other) {
        return device != other.device;
    }

    HSerialController* HSerialPort::getCurrentController() {
        return device->getCurrentController();
    }

    std::string HSerialPort::getDeviceName() const {
        return device->deviceName;
    }

    std::string HSerialPort::getDescription() {
        std::string s = device->getDescription();
        if (s.empty()) {
            HSerialPortsManager& manager = HSerialPortsManager::getInstance();
            manager.refreshDeviceDetails(*device);
            return device->getDescription();
        }
        return s;
    }

    std::string HSerialPort::getHardwareID() {
        std::string s = device->getHardwareID();
        if (s.empty()) {
            HSerialPortsManager& manager = HSerialPortsManager::getInstance();
            manager.refreshDeviceDetails(*device);
            return device->getHardwareID();
        }
        return s;
    }

}
