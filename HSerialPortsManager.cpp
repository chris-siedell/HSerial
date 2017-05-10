//
//  HSerialPortsManager.cpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerialPortsManager.hpp"

#include "HSerialPort.hpp"
#include "HSerialDevice.hpp"


namespace hserial {

    HSerialPortsManager& HSerialPortsManager::getInstance() {
        static HSerialPortsManager instance;
        return instance;
    }


    std::vector<HSerialPort> HSerialPortsManager::getPorts() {
        std::lock_guard<std::mutex> lock(devicesMutex);
        std::vector<serial::PortInfo> portInfoList = serial::list_ports();
        std::vector<HSerialPort> ports;
        for (const serial::PortInfo& portInfo : portInfoList) {
            if (portInfo.port.empty()) continue; // not expected
            // Using emplace_back could save a copy, but would require the HSerialPort device
            //  constructor be public.
            HSerialPort port(getDeviceInternal(portInfo));
            ports.push_back(port);
        }
        return ports;
    }

    HSerialPort HSerialPortsManager::portForDeviceName(const std::string& deviceName) {
        if (deviceName.empty()) throw std::invalid_argument("Device name must not be empty.");
        std::lock_guard<std::mutex> lock(devicesMutex);
        return HSerialPort(getDeviceInternal(deviceName));
    }

    std::shared_ptr<HSerialDevice> HSerialPortsManager::getDevice(const std::string& deviceName) {
        std::lock_guard<std::mutex> lock(devicesMutex);
        return getDeviceInternal(deviceName);
    }

    void HSerialPortsManager::refreshDeviceDetails(HSerialDevice& device) {
        std::lock_guard<std::mutex> lock(devicesMutex);
        std::vector<serial::PortInfo> portInfoList = serial::list_ports();
        for (const serial::PortInfo& info : portInfoList) {
            if (info.port == device.deviceName) {
                device.setDescriptionAndHardwareID(info.description, info.hardware_id);
                return;
            }
        }
    }

    std::shared_ptr<HSerialDevice> HSerialPortsManager::getDeviceInternal(const std::string& deviceName) {
        // Assumes devicesMutex is locked.
        if (deviceName.empty()) throw std::invalid_argument("Device name must not be empty.");
        try {
            return devices.at(deviceName);
        } catch (const std::out_of_range& e) {
            std::shared_ptr<HSerialDevice> d = HSerialDevice::createShared(deviceName, "", "");
            devices.insert({deviceName, d});
            return d;
        }
    }

    std::shared_ptr<HSerialDevice> HSerialPortsManager::getDeviceInternal(const serial::PortInfo& portInfo) {
        // Assumes devicesMutex is locked.
        try {
            return devices.at(portInfo.port);
        } catch (const std::out_of_range& e) {
            std::shared_ptr<HSerialDevice> d = HSerialDevice::createShared(portInfo.port, portInfo.description, portInfo.hardware_id);
            devices.insert({portInfo.port, d});
            return d;
        }
    }

}