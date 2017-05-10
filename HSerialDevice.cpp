//
//  HSerialDevice.cpp
//  HSerial
//
//  Created by admin on 3/9/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerialDevice.hpp"

#include "HSerialController.hpp"

// todo: fix
// Note: in the current implementation there is no way to remove devices from the devices list. Once
//  created, HSerialDevice instances are never destroyed until shutdown. There are plans to fix
//  this eventually, but it is not a significant immmediate problem.

namespace hserial {

    std::shared_ptr<HSerialAccess> HSerialDevice::getAccess() {
        try {
            return std::shared_ptr<HSerialAccess>(access);
        } catch (const std::bad_weak_ptr& e) {
            std::shared_ptr<HSerialAccess> a(HSerialAccess::createShared(deviceName));
            access = a;
            return a;
        }
    }

    std::string HSerialDevice::getDescription() {
        std::lock_guard<std::mutex> lock(detailsMutex);
        return description;
    }

    std::string HSerialDevice::getHardwareID() {
        std::lock_guard<std::mutex> lock(detailsMutex);
        return hardwareID;
    }

    void HSerialDevice::setDescriptionAndHardwareID(const std::string& _description, const std::string& _hardwareID) {
        std::lock_guard<std::mutex> lock(detailsMutex);
        description = _description;
        hardwareID = _hardwareID;
    }

    HSerialController* HSerialDevice::getCurrentController() {
        try {
            std::shared_ptr<HSerialAccess> a(access);
            return a->state_currentController.load();
        } catch (const std::bad_weak_ptr& e) {
            return NULL;
        }
    }


#pragma mark - Construction/Destruction

    HSerialDevice::HSerialDevice(const std::string& deviceName, const std::string& _description, const std::string& _hardwareID) : deviceName(deviceName) {
        description = _description;
        hardwareID = _hardwareID;
    }

    HSerialDevice::~HSerialDevice() {}


#pragma mark - shared_ptr Creation

    /*!
     \brief Used by createShared.

     This struct is part of a hack for using a `shared_ptr` with a class with a private constructor.

     MakeSharedEnabler and createShared allow creating a shared pointer with
     std::make_shared for a class with a private constructor.
     Based on [this StackOverflow answer](http://stackoverflow.com/a/20961251).

     Internal use only.
     */
    struct HSerialDevice::MakeSharedEnabler : public HSerialDevice {
        MakeSharedEnabler(const std::string& deviceName, const std::string& description, const std::string& hardwareID) : HSerialDevice(deviceName, description, hardwareID) {}
    };

    std::shared_ptr<HSerialDevice> HSerialDevice::createShared(const std::string& deviceName, const std::string& description, const std::string& hardwareID) {
        return std::make_shared<MakeSharedEnabler>(deviceName, description, hardwareID);
    }

}