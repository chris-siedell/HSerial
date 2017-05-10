//
//  HSerialExceptions.hpp
//  HSerial
//
//  Created by admin on 3/10/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialExceptions_hpp
#define HSerialExceptions_hpp

#include <string>
#include <stdexcept>


namespace hserial {

    class HSerialController;

    /*!
     \brief Thrown when an inactive serial controller calls an access method.
     */
    class NotActiveController : public std::runtime_error {
    public:
        NotActiveController(const std::string& description) : std::runtime_error(description) {}
    };

// todo: consider the problem of private delegate controllers being exposed by
//       ControllerRefuses. See the proposal in HSerialController.hpp.

    /*!
     \brief Thrown when a controller refuses to be removed from the access list or made inactive.
     
     This exception should be thrown only from a controller's willMakeInactive or willRemove
     callbacks. It is used to cancel the controller change. The exception then propagates to
     the makeActive, makeInactive, or removeFromAccess function call responsible for requesting
     the controller change.

     \see HSerialController::willMakeInactive, HSerialController::willRemove
    */
    class ControllerRefuses : public std::runtime_error {
    public:
        ControllerRefuses(HSerialController& controller, const std::string& description) : refusingController(controller), std::runtime_error(description) {}
        const HSerialController& refusingController;
    };
    
}
#endif /* HSerialExceptions_hpp */
