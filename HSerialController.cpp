//
//  HSerialController.cpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#include "HSerialController.hpp"

#include "HSerialDevice.hpp"
#include "HSerialAccess.hpp"
#include "HSerialExceptions.hpp"


namespace hserial {
    
    
#pragma mark - Constructor/Destructor

    HSerialController::HSerialController(HSerialPort _port) : port(std::move(_port)) {
        access = port.device->getAccess();
    }

    HSerialController::HSerialController(const std::string& deviceName) : port(deviceName) {
        access = port.device->getAccess();
    }

    HSerialController::~HSerialController() {
        // todo: remove
        isActive();
    }


#pragma mark - Public Methods

    HSerialPort HSerialController::getPort() const {
        return port;
    }
    
    std::string HSerialController::getDeviceName() const {
        return port.device->deviceName;
    }

    std::string HSerialController::getDescription() const {
        std::stringstream ss;
        ss << getControllerType() << " for '" << port.device->deviceName << "' (" << this << ")";
        return ss.str();
    }


#pragma mark - Access Management

    bool HSerialController::isActive() const {
        return access->isActive(*this);
    }

    void HSerialController::makeActive() {
        access->makeActive(*this);
    }

    void HSerialController::makeInactive() {
        access->makeInactive(*this);
    }

    void HSerialController::removeFromAccess() {
        access->removeFromAccess(*this);
    }


#pragma mark - Access Functions

    void HSerialController::open() {
        access->open(*this);
    }

    void HSerialController::ensureOpen() {
        access->ensureOpen(*this);
    }

    bool HSerialController::isOpen() const {
        return access->isOpen(*this);
    }

    void HSerialController::close() {
        access->close(*this);
    }

    size_t HSerialController::available() {
        return access->available(*this);
    }

    bool HSerialController::waitReadable() {
        return access->waitReadable(*this);
    }

    void HSerialController::waitByteTimes(size_t count) {
        access->waitByteTimes(*this, count);
    }

    size_t HSerialController::read(uint8_t* buffer, size_t size) {
        return access->read(*this, buffer, size);
    }

    size_t HSerialController::read(std::vector<uint8_t>& buffer, size_t size) {
        return access->read(*this, buffer, size);
    }

    size_t HSerialController::read(std::string& buffer, size_t size) {
        return access->read(*this, buffer, size);
    }

    std::string HSerialController::read(size_t size) {
        return access->read(*this, size);
    }

    size_t HSerialController::readline(std::string& buffer, size_t size, std::string eol) {
        return access->readline(*this, buffer, size, eol);
    }

    std::string HSerialController::readline(size_t size, std::string eol) {
        return access->readline(*this, size, eol);
    }

    std::vector<std::string> HSerialController::readlines(size_t size, std::string eol) {
        return access->readlines(*this, size, eol);
    }

    size_t HSerialController::write(const uint8_t* data, size_t size) {
        return access->write(*this, data, size);
    }

    size_t HSerialController::write(const std::vector<uint8_t>& data) {
        return access->write(*this, data);
    }

    size_t HSerialController::write(const std::string &data) {
        return access->write(*this, data);
    }

    void HSerialController::setBaudrate(uint32_t baudrate, bool onlyIfDifferent) {
        access->setBaudrate(*this, baudrate, onlyIfDifferent);
    }

    uint32_t HSerialController::getBaudrate() const {
        return access->getBaudrate(*this);
    }

    void HSerialController::setTimeout(serial::Timeout& timeout, bool onlyIfDifferent) {
        access->setTimeout(*this, timeout, onlyIfDifferent);
    }

    serial::Timeout HSerialController::getTimeout() const {
        return access->getTimeout(*this);
    }

    void HSerialController::setBytesize(serial::bytesize_t bytesize, bool onlyIfDifferent) {
        access->setBytesize(*this, bytesize, onlyIfDifferent);
    }

    serial::bytesize_t HSerialController::getBytesize() const {
        return access->getBytesize(*this);
    }

    void HSerialController::setParity(serial::parity_t parity, bool onlyIfDifferent) {
        access->setParity(*this, parity, onlyIfDifferent);
    }

    serial::parity_t HSerialController::getParity() const {
        return access->getParity(*this);
    }

    void HSerialController::setStopbits(serial::stopbits_t stopbits, bool onlyIfDifferent) {
        access->setStopbits(*this, stopbits, onlyIfDifferent);
    }

    serial::stopbits_t HSerialController::getStopbits() const {
        return access->getStopbits(*this);
    }

    void HSerialController::setFlowcontrol(serial::flowcontrol_t flowcontrol, bool onlyIfDifferent) {
        access->setFlowcontrol(*this, flowcontrol, onlyIfDifferent);
    }

    serial::flowcontrol_t HSerialController::getFlowcontrol() const {
        return access->getFlowcontrol(*this);
    }

    void HSerialController::setSettings(uint32_t baudrate, serial::Timeout timeout, serial::bytesize_t bytesize,
                                        serial::parity_t parity, serial::stopbits_t stopbits, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent) {
        access->setSettings(*this, baudrate, timeout, bytesize, parity, stopbits, flowcontrol, onlyIfDifferent);
    }

    void HSerialController::flush() {
        access->flush(*this);
    }

    void HSerialController::flushInput() {
        access->flushInput(*this);
    }

    void HSerialController::flushOutput() {
        access->flushOutput(*this);
    }

    void HSerialController::sendBreak(int duration) {
        access->sendBreak(*this, duration);
    }

    void HSerialController::setBreak(bool level) {
        access->setBreak(*this, level);
    }

    void HSerialController::setRTS(bool level) {
        access->setRTS(*this, level);
    }

    void HSerialController::setDTR(bool level) {
        access->setDTR(*this, level);
    }

    bool HSerialController::waitForChange() {
        return access->waitForChange(*this);
    }

    bool HSerialController::getCTS() {
        return access->getCTS(*this);
    }

    bool HSerialController::getDSR() {
        return access->getDSR(*this);
    }

    bool HSerialController::getRI() {
        return access->getRI(*this);
    }

    bool HSerialController::getCD() {
        return access->getCD(*this);
    }


#pragma mark - Delegation

    void HSerialController::registerDelegate(HSerialController* delegate) {
        if (delegate == NULL) {
            throw std::invalid_argument("A delegate must be a non-NULL controller.");
        }
        if (this == delegate) {
            throw std::invalid_argument("A controller cannot delegate to itself.");
        }
        if (hasAsFirstDegreeDelegate(delegate)) {
            throw std::invalid_argument("Cannot add a delegate more than once to the same controller.");
        }
        if (delegate->hasAsDelegateOrSubdelegate(this)) {
            throw std::invalid_argument("Delegation cycles are not allowed.");
        }
        delegates.push_back(delegate);
    }


#pragma mark - Transition Callbacks

    // Note: the other callbacks have empty default implementations.

    void HSerialController::willMakeInactive() {
        blockAccessCalls();
        bool success = waitForAllAccessCallsToReturn(std::chrono::milliseconds(1500));
        if (!success) {
            throw ControllerRefuses(*this, "Access calls have not returned.");
        }
    }


#pragma mark - Transition Utilities

    void HSerialController::blockAccessCalls() {
        access->blockAccessCalls(*this);
    }

    void HSerialController::unblockAccessCalls() {
        access->unblockAccessCalls(*this);
    }

    bool HSerialController::waitForAllAccessCallsToReturn(const std::chrono::milliseconds& timeout) {
        return access->waitForAllAccessCallsToReturn(*this, timeout);
    }


#pragma mark - For Friends

    std::vector<HSerialController*> HSerialController::getControllersList() {
        std::vector<HSerialController*> list {this};
        size_t numAppended;
        int degree = 1;
        do {
            numAppended = appendDelegatesOfDegree(list, degree);
            degree += 1;
        } while (numAppended > 0);
        return list;
    }


#pragma mark - Internal Stuff

    bool HSerialController::hasAsFirstDegreeDelegate(const HSerialController* otherController) {
        if (!otherController) return false; // not strictly necessary since NULL should not be in the list
        return std::find(delegates.begin(), delegates.end(), otherController) != delegates.end();
    }

    bool HSerialController::hasAsDelegateOrSubdelegate(const HSerialController* otherController) {
        if (!otherController) return false;
        for (HSerialController* x : delegates) {
            if (x == otherController) return true;
            else {
                bool success = x->hasAsDelegateOrSubdelegate(otherController);
                if (success) return true;
            }
        }
        return false;
    }

    size_t HSerialController::appendDelegatesOfDegree(std::vector<HSerialController*>& list, int degree) {
        if (degree < 0) {
            return 0;
        } else if (degree == 0) {
            list.push_back(this);
            return 1;
        } else {
            degree -= 1;
            size_t count = 0;
            for (HSerialController* x : delegates) {
                count += x->appendDelegatesOfDegree(list, degree);
            }
            return count;
        }
    }

}

