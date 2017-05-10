//
//  HSerialController.hpp
//  HSerial
//
//  Created by admin on 3/8/17.
//  Copyright © 2017 Chris Siedell. All rights reserved.
//

#ifndef HSerialController_hpp
#define HSerialController_hpp

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

#include <serial/serial.h>


#include "HSerialPort.hpp"


namespace hserial {

    // todo: Consider the following
    //       replace
    //                  void willRemove()
    //                  void willMakeInactive()
    //       with
    //                  bool allowRemove(std::string& refusalReason) noexcept
    //                  bool allowMakeInactive(std::string& refusalReason) noexcept
    //       This would...
    //          - enforce having ControllerRefuses as the exception for refusals,
    //          - allow HSerial to have a standardized format for ControllerRefuses, and
    //          - allow HSerial to hide delegate controllers in the ControllerRefuses exception.
    //            Since controllers do not know if they are delegates they can not hide themselves
    //            as the refusing controller, allowing private delegates to be exposed via
    //            the exception.
    //       Negatives...
    //          - Would eliminate other exceptions from willMakeInactive. However,
    //            the calling code ultimately responsible for catching the exception (a
    //            makeActive call on another controller) should probably not be assuming things
    //            about the controller that needs to be made inactive.

    // todo: Consider the idea that a controller may want to base its decision on whether to
    //       become inactive on the identity of the controller making the request. Essentially,
    //       this is the inverse of delegation. Is this sensible? A good idea?

    // todo: Consider the idea of temporary handoff between controllers. But isn't this a
    //       higher-level concept that users should implement?

    class HSerialAccess;

    /*!
     \brief The base class for objects that use the serial port.
     
     Interacting with the serial port in the %HSerial Library is done through serial port
     controllers. All serial port controllers are derived from this class.
     */
    class HSerialController {


    public:

        /*!
         \brief Identifies the type of controller, which is typically the class name
         (e.g. `"HSerial"`).

         Subclasses of HSerialController must implement this function, and should typically
         return the class name (possibly including the namespace, if useful).
         The function should always return the same value.
         */
        virtual std::string getControllerType() const = 0;

        /*!
         \brief Returns a copy of the port associated with this controller.
         */
        HSerialPort getPort() const;

        /*!
         \brief Returns the name of the serial device associated with this controller.
         */
        std::string getDeviceName() const;

        /*!
         \brief Returns a string in the form
         <tt>"<controllerType> for '<deviceName>' (<instance address>)"</tt>.
         */
        std::string getDescription() const;

        /*!
         A subclass must implement the destructor and it must ensure that the controller is not in
         the access list before proceeding with destruction. The removeFromAccess() function can
         be used to remove the controller from the access list if it is the current controller.

         (If a destroyed controller is in the access list but it is not the current controller then
         the delegating controller is not fulfilling its responsibility to keep the
         delegate alive.)

         __Important__: If a destroyed controller fails to remove itself from the access list then
         the program will crash. See the [implementation notes](@ref destructor).
         */
        virtual ~HSerialController();

        HSerialController() = delete;
        HSerialController(const HSerialController&) = delete;
        HSerialController& operator=(const HSerialController&) = delete;
        HSerialController(HSerialController&&) = delete;
        HSerialController& operator=(HSerialController&&) = delete;

    protected:

        /*!
         \brief Creates a controller for the given port.
         
         The controller keeps the copy of the port object.
         
         The constructors are protected since HSerialController is an abstract base class.
         */
        HSerialController(HSerialPort port);

        /*!
         \brief Creates a controller for the given port, identified by its device name.

         This will always succeed as long as `deviceName` is not empty.
         
         The constructors are protected since HSerialController is an abstract base class.
         */
        HSerialController(const std::string& deviceName);


#pragma mark - Access Management

        /*!
         \anchor controller-access-management
         \name Access Management

         These functions manage the controller's access to the port.
         */
        /// \{

        /*!
         \brief Indicates if the controller is active.
         
         An active controller has exclusive use of the serial port until it agrees to relinquish
         it.

         Beware that in a multithreaded environment this value may change at any time.
         The only way to ensure that a controller stays active is
         for the controller to refuse changes in its willMakeInactive() or willRemove() callbacks.

         \see makeActive, makeInactive, removeSelfIfCurrent
         */
        bool isActive() const;

        /*!
         \brief Makes the controller active.

         If the controller is already active this function does nothing.
         
         Calling code should always catch a ControllerRefuses exception.
         
         If an exception is thrown the controller may or may not be active, depending on
         whether the error occurred before or after the controller change. If the exception
         is ControllerRefuses (and the the controller was implemented correctly) then the error
         occurred before the controller change.

         \throws hserial::ControllerRefuses if the existing active controller refuses to relinquish
         the active controller role, or if a current controller change is required and any
         controller in the access list refuses to be removed.
         \see isActive, makeInactive, removeSelfIfCurrent
         */
        void makeActive();

        /*!
         \brief Makes the controller inactive.

         If the controller is already inactive this function does nothing.
         
         Calling code should always catch a ControllerRefuses exception.
         
         If an exception is thrown then the controller is still active.

         \throws hserial::ControllerRefuses Potentially thrown by the controller's
         willMakeInactive() callback.
         \see isActive, makeActive, removeSelfIfCurrent
         */
        void makeInactive();

        /*!
         \brief Removes the controller from the access list.

         If the controller is the current controller this function will attempt to remove it and its
         delegates from the access list -- in other words, it will set the current controller to
         `NULL`. A side effect of this is that any active controller will be made inactive.
         
         If this controller is in the access list as a delegate or subdelegate an exception is
         thrown since it can't be removed.
         
         If the controller is not in the access list then this function does nothing.

         This function is intended to be used in controller's destructor (see the
         [implementation notes](\ref destructor)),
         although it can be used elsewhere.
         
         Calling code should always catch a ControllerRefuses exception.

         \throws hserial::ControllerRefuses Thrown if the current active controller refuses to
         relinquish the active role, or if any controller on the access list refuses to be removed.
         The controller that throws this exception will be the calling controller, a delegate,
         or a subdelegate.
         \throws std::logic_error Thrown if the controller is in the access list but is not the
         current controller (that is, it is a delegate or subdelegate of the current controller).
         \see isActive, makeActive, makeInactive
         */
        void removeFromAccess();
        
        /// \} /Access Management


#pragma mark - Access Functions

        /*!
         \anchor controller-access-functions
         \name Access Functions
         
         These are the functions used to work with the serial port. Most of these are identical to
         the functions of a `serial::Serial` object.
         The only difference is that they will fail (throw NotActiveController) if
         the controller is not active (see [Access Management](\ref controller-access-management)).
        */
        /// \{

        /*!
         \brief Opens the serial port.

         \throws std::invalid_argument
         \throws serial::SerialException Thrown if port is already open.
         \throws serial::IOException
         \throws hserial::NotActiveController 
         \see ensureOpen, isOpen, close
         */
        void open();

        /*!
         \brief Opens the serial port if it is not already open.

         This function is a convenient alternative to open(). If the port is already open this 
         function does nothing -- the open() function would throw an exception.

         \throws hserial::NotActiveController 
         \see open, isOpen, close
         */
        void ensureOpen();

        /*!
         \brief Indicates if the port is open.
         \returns `true` if the port is open, `false` otherwise.
         \throws hserial::NotActiveController 
         \see open, ensureOpen, close
         */
        bool isOpen() const;
        
        /*! 
         \brief Closes the serial port.
         \throws hserial::NotActiveController 
         \see open, ensureOpen, isOpen
         */
        void close();

        /*!
         \brief Returns the number of characters in the buffer.
         \throws hserial::NotActiveController 
         \see waitReadable, waitByteTimes
         */
        size_t available();

        /*!
         \brief Blocks until there is serial data to read or timeout occurs.
         
         This function will wait until `read_timeout_constant` milliseconds have elapsed.
         
         Note: `serial::Serial::waitReadable` is not implemented on Windows.

         This function is probably not MT-Safe. See the [design notes](\ref multithreading-serial).

         \returns `true` if the function exits with the port in a readable state, `false`otherwise
         (due to timeout or select interruption).
         \throws hserial::NotActiveController
         \see available, waitByteTimes
         */
        bool waitReadable();

        /*!
         \brief Blocks for the time it would take to transmit `count` bytes.
         
         This function may be used in conjunction with waitReadable() to read larger blocks of data
         from the port.
         
         Note: `serial::Serial::waitByteTimes` is not implemented on Windows.

         This function is probably not MT-Safe. See the [design notes](\ref multithreading-serial).

         \throws hserial::NotActiveController
         \see available, waitReadable
         */
        void waitByteTimes(size_t count);

        /*!
         \brief Reads bytes from the serial port into a buffer.

         The read function will return in one of three cases:
         
         - The number of requested bytes was read.
           - In this case the number of bytes requested will match the size_t
             returned by read.
         - A timeout occurred, in this case the number of bytes read will not
         match the amount requested, but no exception will be thrown.  One of
         two possible timeouts occurred:
           - The inter byte timeout expired, this means that number of
             milliseconds elapsed between receiving bytes from the serial port
             exceeded the inter byte timeout.
           - The total timeout expired, which is calculated by multiplying the
             read timeout multiplier by the number of requested bytes and then
             added to the read timeout constant.  If that total number of
             milliseconds elapses after the initial call to read a timeout will
             occur.
         - An exception occurred, in this case an actual exception will be thrown.

         \param buffer An uint8_t array of at least the requested size.
         \param size A size_t defining how many bytes to be read.
         \returns A size_t representing the number of bytes read as a result of the call to read.
         \throws serial::PortNotOpenedException Thrown if the port is not open.
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see setTimeout
         */
        size_t read(uint8_t* buffer, size_t size);

        /*!
         \brief Reads bytes from the serial port into a buffer.

         \param buffer A reference to a std::vector of uint8_t.
         \param size A size_t defining how many bytes to be read.
         \returns A size_t representing the number of bytes read as a result of the call to read.
         \throws serial::PortNotOpenedException Thrown if the port is not open.
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        size_t read(std::vector<uint8_t>& buffer, size_t size = 1);

        /*! 
         \brief Reads bytes from the serial port into a buffer.

         \param buffer A reference to a std::string.
         \param size A size_t defining how many bytes to be read.
         \returns A size_t representing the number of bytes read as a result of the call to read.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        size_t read(std::string& buffer, size_t size = 1);

        /*!
         \brief Reads bytes from the serial port and returns them as a string.
         
         \param size A size_t defining how many bytes to be read.
         \returns A std::string containing the data read from the port.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        std::string read(size_t size = 1);

        /*!
         \brief Reads in a line or until a given delimiter has been processed.
         
         \param buffer A std::string reference used to store the data.
         \param size A maximum length of a line, defaults to 65536 (2^16).
         \param eol A string to match against for the EOL.
         \returns A size_t representing the number of bytes read.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        size_t readline(std::string& buffer, size_t size = 65536, std::string eol = "\n");

        /*!
         \brief Reads in a line or until a given delimiter has been processed.
         
         \param size A maximum length of a line, defaults to 65536 (2^16).
         \param eol A string to match against for the EOL.
         \returns A std::string containing the line.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        std::string readline(size_t size = 65536, std::string eol = "\n");

        /*!
         \brief Reads in multiple lines until the serial port times out.

         This requires a timeout > 0 before it can be run. It will read until a timeout occurs and 
         return a list of strings.

         \param size A maximum length of combined lines, defaults to 65536 (2^16).
         \param eol A string to match against for the EOL.

         \returns A vector<string> containing the lines.

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController 
         \see read(uint8_t* buffer, size_t size), setTimeout
         */
        std::vector<std::string> readlines(size_t size = 65536, std::string eol = "\n");

        /*!
         \brief Writes a string to the serial port.

         \param data A const reference containing the data to be written to the serial port.
         \param size A size_t that indicates how many bytes should be written from the given data
         buffer.

         \returns A size_t representing the number of bytes actually written to the serial port.

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws serial::IOException
         \throws hserial::NotActiveController 
         \see setTimeout
         */
        size_t write(const uint8_t* data, size_t size);

        /*!
         \brief Writes a string to the serial port.

         \param data A const reference containing the data to be written to the serial port.

         \return A size_t representing the number of bytes actually written to the serial port.

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws serial::IOException
         \throws hserial::NotActiveController 
         \see setTimeout
         */
        size_t write(const std::vector<uint8_t>& data);

        /*!
         \brief Writes a string to the serial port.

         \param data A const reference containing the data to be written to the serial port.

         \returns A size_t representing the number of bytes actually written to the serial port.

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws serial::IOException
         \throws hserial::NotActiveController 
         \see setTimeout
         */
        size_t write(const std::string &data);

        /*!
         \brief Sets the baudrate of the serial port.

         Possible baudrates depend on the system but some safe baudrates include:
         110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 56000,
         57600, and 115200.
         Some other baudrates that are supported by some systems:
         128000, 153600, 230400, 256000, 460800, and 921600.

         \param baudrate The baudrate to use for reading and writing.
         \param onlyIfDifferent If this flag is true the serial port's baudrate setting
         will be set only if the current reported value (from getBaudrate) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getBaudrate
         */
        void setBaudrate(uint32_t baudrate, bool onlyIfDifferent = false);

        /*!
         \brief Returns the baudrate of the serial port.

         \throws hserial::NotActiveController
         \see setBaudrate
         */
        uint32_t getBaudrate() const;
        
        /*!
         \brief Sets the timeout for reads and writes using a `Timeout` struct.

         There are two timeout conditions described here:

         - The inter byte timeout:
           - The inter_byte_timeout component of serial::Timeout defines the
             maximum amount of time, in milliseconds, between receiving bytes on
             the serial port that can pass before a timeout occurs.  Setting this
             to zero will prevent inter byte timeouts from occurring.
         - Total time timeout:
           - The constant and multiplier component of this timeout condition,
             for both read and write, are defined in serial::Timeout.  This
             timeout occurs if the total time since the read or write call was
             made exceeds the specified time in milliseconds.
           - The limit is defined by multiplying the multiplier component by the
             number of requested bytes and adding that product to the constant
             component.  In this way if you want a read call, for example, to
             timeout after exactly one second regardless of the number of bytes
             you asked for then set the read_timeout_constant component of
             serial::Timeout to 1000 and the read_timeout_multiplier to zero.
             This timeout condition can be used in conjunction with the inter
             byte timeout condition with out any problems, timeout will simply
             occur when one of the two timeout conditions is met.  This allows
             users to have maximum control over the trade-off between
             responsiveness and efficiency.

         Read and write functions will return in one of three cases: when the
         reading or writing is complete, when a timeout occurs, or when an
         exception occurs.

         \param timeout A `serial::Timeout` struct containing the inter byte
         timeout, and the read and write timeout constants and multipliers.
         \param onlyIfDifferent If this flag is true the serial port's timeout setting
         will be set only if the current reported value (from getTimeout) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getTimeout
         */
        void setTimeout(serial::Timeout& timeout, bool onlyIfDifferent = false);

        /*!
         \brief Gets the timeout for reads and writes.

         \returns A `serial::Timeout` struct containing the inter_byte_timeout, and read
         and write timeout constants and multipliers.

         \throws hserial::NotActiveController 
         \see setTimeout
         */
        serial::Timeout getTimeout() const;

        /*!
         \brief Sets the bytesize for the serial port.

         \param bytesize Size of each byte in the serial transmission of data,
         Possible values are: `serial::fivebits`, `serial::sixbits`,
         `serial::sevenbits`, and `serial::eightbits`.
         The default is `serial::eightbits`.
         \param onlyIfDifferent If this flag is true the serial port's bytesize setting
         will be set only if the current reported value (from getBytesize) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getBytesize
         */
        void setBytesize(serial::bytesize_t bytesize, bool onlyIfDifferent = false);

        /*!
         \brief Gets the bytesize for the serial port.

         \throws hserial::NotActiveController
         \see setBytesize
         */
        serial::bytesize_t getBytesize() const;

        /*!
         \brief Sets the parity for the serial port.
         
         Possible options:
         
         Parity Value            | Parity Bit
         ------------------------|-----------------
         `serial::parity_none`   | not included
         `serial::parity_odd`    | gives odd sum
         `serial::parity_even`   | gives even sum
         `serial::parity_mark`   | always 1
         `serial::parity_space`  | always 0

         Not all options available on all platforms.

         \param parity The parity option to use. The default is `serial::parity_none`.
         \param onlyIfDifferent If this flag is true the serial port's parity setting
         will be set only if the current reported value (from getParity) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getParity
         */
        void setParity(serial::parity_t parity, bool onlyIfDifferent = false);

        /*!
         \brief Gets the parity for the serial port.
         \throws hserial::NotActiveController
         \see setParity
         */
        serial::parity_t getParity() const;

        /*!
         \brief Sets the number of stop bits.
         
         Not all values are available on all platforms.

         \param The number of stop bits to use. Possible values are `serial::stopbits_one`,
         `serial::stopbits_one_point_five`, and `serial::stopbits_two`.
         The default is `serial::stopbits_one`.

         \param onlyIfDifferent If this flag is true the serial port's stopbits setting
         will be set only if the current reported value (from getStopbits) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getStopbits
         */
        void setStopbits(serial::stopbits_t stopbits, bool onlyIfDifferent = false);

        /*!
         \brief Gets the number of stop bits.
         \throws hserial::NotActiveController
         \see setStopbits
         */
        serial::stopbits_t getStopbits() const;

        /*!
         \brief Sets the flow control method.

         \param flowcontrol The method of flow control to use.
         Possible values are `serial::flowcontrol_none`, `serial::flowcontrol_software`, and
         `serial::flowcontrol_hardware`.
         The default is `serial::flowcontrol_none`.

         \param onlyIfDifferent If this flag is true the serial port's flowcontrol setting
         will be set only if the current reported value (from getFlowcontrol) is different
         than the given value. This avoids unnecessary port reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see getFlowcontrol
         */
        void setFlowcontrol(serial::flowcontrol_t flowcontrol, bool onlyIfDifferent = false);

        /*!
         \brief Gets the flow control method.
         \throws hserial::NotActiveController
         \see setFlowcontrol
         */
        serial::flowcontrol_t getFlowcontrol() const;

        /*!
         \brief Sets all of the settings of a serial port at once.
         
         This is a convenience function for applying all of the settings of a serial port in
         a single call. Using this function is like calling each of the setting functions (e.g.
         setBaudrate, setTimeout, etc.) in sequence.

         The settings are applied in the order of the arguments. If applying any setting causes
         an exception to be thrown then the later settings will not be applied. For example, if
         the bytesize is invalid then std::invalid_argument will be thrown and parity,
         stopbits, and flowcontrol will never be applied.
         
         \param onlyIfDifferent If this flag is true a given setting will be applied only if the
         current value is different than the given value. This avoids unnecessary port
         reconfigurations.

         \throws std::invalid_argument
         \throws serial::IOException
         \throws hserial::NotActiveController
         \see setBaudrate, setTimeout, setBytesize, setParity, setStopbits, setFlowcontrol
         */
        void setSettings(uint32_t baudrate, serial::Timeout timeout, serial::bytesize_t bytesize,
                         serial::parity_t parity, serial::stopbits_t stopbits, serial::flowcontrol_t flowcontrol, bool onlyIfDifferent = false);

        /*!
         \brief Flushes the input and output buffers.
         \throws serial::PortNotOpenedException
         \throws hserial::NotActiveController
         \see flushInput, flushOutput
         */
        void flush();

        /*! 
         \brief Flushes the input buffer only.

         Note: `serial::Serial::flushInput` is not implemented on Windows.

         \throws serial::PortNotOpenedException
         \throws hserial::NotActiveController
         \see flush, flushOutput
         */
        void flushInput();

        /*!
         \brief Flushes the output buffer only.

         Note: `serial::Serial::flushOutput` is not implemented on Windows.

         \throws serial::PortNotOpenedException
         \throws hserial::NotActiveController
         \see flush, flushInput
         */
        void flushOutput();

        /*! 
         \brief Sends the break signal.
         
         See tcsendbreak(3).
         
         Note: `serial::Serial::sendBreak` is not implemented on Windows.

         \throws serial::PortNotOpenedException
         \throws hserial::NotActiveController 
         \see setBreak
         */
        void sendBreak(int duration);

        /*!
         \brief Sets the break level.
         
         Defaults to true (low signal).

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see sendBreak
         */
        void setBreak(bool level);

        /*!
         \brief Sets the RTS (request to send) control line level.

         Defaults to true (low signal).

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         */
        void setRTS(bool level);

        /*!
         \brief Sets the DTR (data terminal ready) control line level.
         
         Defaults to true (low signal).

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         */
        void setDTR(bool level);

        /*!
         \brief Blocks until one of input control lines change.

         This function blocks until the CTS (clear to send), DSR (data set ready), 
         RI (ring indicator), or CD (carrier detect) lines change or something interrupts it.

         This function will throw an exception if an error occurs while waiting.
         You can check the status of CTS, DSR, RI, and CD once this returns.
         Uses TIOCMIWAIT via ioctl if available (mostly only on Linux) with a
         resolution of less than +-1ms and as good as +-0.2ms.  Otherwise a
         polling method is used which can give +-2ms.
         
         This function is probably not MT-Safe. See the [design notes](\ref multithreading-serial).
        
         \returns `true` if one of the lines changed, `false` otherwise.

         \throws serial::SerialException
         \throws hserial::NotActiveController 

         \see getCTS, getDSR, getRI, getCD
         */
        bool waitForChange();

        /*!
         \brief Returns the current level of the CTS (clear to send) line.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see waitForChange
         */
        bool getCTS();

        /*!
         \brief Returns the current level of the DSR (data set ready) line.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see waitForChange
         */
        bool getDSR();

        /*!
         \brief Returns the current level of the RI (ring indicator) line.
         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see waitForChange
         */
        bool getRI();

        /*!
         \brief Returns the current status of the CD (carrier detect) line.
         
         Also referred to as DCD (data carrier detect).

         \throws serial::PortNotOpenedException
         \throws serial::SerialException
         \throws hserial::NotActiveController
         \see waitForChange
         */
        bool getCD();

        /// \} /Access Functions
        

#pragma mark - Delegation

        /*!
         \anchor controller-delegation
         \name Delegation
         
         Delegation is a way for a controller to use another controller.
         See the [implementation notes](\ref delegation) for more details.
        */
        /// \{

        /*!
         \brief __For use in constructor only.__ Registers a delegate of the controller.

         Registering a delegate means letting the %HSerial library know that the controller will be
         using another controller as a delegate.

         Requirements and restrictions:
         
         - __Registering delegates is allowed in the constructor only.__ Calling this function at
         any other time will lead to undefined behavior.
         - The delegate must exist for the life of the controller -- the pointer must not become
         invalid.
         - The delegate must not introduce a delegation cycle. A controller cannot delegate to
         itself or to a delegate that ultimately delegates back to the controller.

         The explanation for the first requirement can be found in the
         [design notes](\ref why-register-delegates-in-constructor).

         \throws std::invalid_argument Thrown if `delegate` is `NULL`, has already been added, is
         identical to the controller, or if adding it would create a cycle
         (the delegate or a subdelegate delegating back to the controller).
         */
        void registerDelegate(HSerialController* delegate);

        /// \} /Delegation


#pragma mark - Transition Callbacks

        /*!
         \name Transition Callbacks
         
         These functions are called by the %HSerial library when a controller is being added or
         removed from the access list, and when a controller is being made active or inactive.
         */
        /// \{

        /*!
         \brief Called before the controller is removed from the access list.

         The main purpose of this callback is to give a delegating controller the ability to
         allow or refuse a current controller change. Controllers that don't use delegation probably
         don't need to implement this callback (or any of the adding or removing callbacks).

         The controller can cancel the change by throwing ControllerRefuses.
         This exception will propagate back to the function call that was responsible for initiating
         the change (e.g. makeActive). Any exception will have the same effect, but the
         initiating code may not be expecting anything other than ControllerRefuses.

         This callback is made for every controller on the access list. The current controller
         has its callback made first, but after that the order is undefined.

         Even if the controller accepts the removal by returning normally the change may still
         be cancelled by another controller on the access list, or by the active controller if it
         refuses to become inactive. In this case didCancelRemove() will be called.
         
         So if this callback returns normally then it is guaranteed that either didCancelRemove() or
         didRemove() will be called. This arrangement allows a controller to provisionally cancel an
         action and then complete the cancellation in didRemove(), or reverse the cancellation in
         didCancelRemove().
         
         A controller is allowed to use its delegates during this callback. That is, active
         controller changes -- but not current controller changes -- are allowed in willRemove().

         The default implementation is empty.
         
         \see didCancelRemove, didRemove, didAdd
         */
        virtual void willRemove() {}

        /*!
         \brief Called when the controller is going to be made inactive.

         __Important__: This is the only callback which must do something. The implementation must
         - block access calls with blockAccessCalls(), and
         - ensure that all access calls have returned (waitForAllAccessCallsToReturn() can be used
         for this).

         Returning without correctly implementing this callback will cause the requested
         transition to fail with a std::logic_error. didCancelMakeInactive will be called in this
         case.

         The controller can cancel the change by throwing ControllerRefuses.
         This exception will propagate back to the function call that was responsible for initiating
         the change (e.g. makeActive). Any exception will have the same effect, but the
         initiating code may not be expecting anything other than ControllerRefuses.

         Guarantee: if this function returns then either didMakeInactive or didCancelMakeInactive
         will be called.

         A controller may require that a mutex be locked throughout the transition -- see HSerial
         for an example. The transition actually occurs after this callback returns. Therefore,
         using a scoped based lock is not sufficient. This is why the two post-transition
         callbacks -- didMakeInactive and didCancelMakeInactive -- exist. A mutex locked in this
         function can be unlocked in those two callbacks.
         (All callbacks are called on the same thread.)

         willRemove() is called before willMakeInactive() for a controller that is simultaneously
         being removed and made inactive. This order is perhaps counterintuitive
         since didAdd() is called before didMakeActive() in the opposite situation.
         This ordering allows a delegating controller to cancel a change even when a
         delegate would allow it.

         If an exception is thrown while the access calls are blocked they will automatically be
         unblocked.

         Here is the default implementation:
         \code{.cpp}
         void HSerialController::willMakeInactive() {
            blockAccessCalls();
            bool success = waitForAllAccessCallsToReturn(std::chrono::milliseconds(1500));
            if (!success) {
                throw ControllerRefuses(*this, "Access calls have not returned.");
            }
         }
         \endcode

         \see didMakeInactive, didCancelMakeInactive
         */
        virtual void willMakeInactive();

        /*!
         \brief Called before the controller is made active.
         
         This callback exists so that a controller can have a mutex locked during the transition.
         This allows synchronizing the state of the controller to its isActive setting.
         
         Suppose the controller has a property that must always be a certain value if the controller
         is active. One way to enforce this rule is to have the mutex that protects that property
         locked during the transition. So other threads will never see the property and the
         isActive property in an inconsistent state.
         
         If this callback is called then didMakeActive will be called.

         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.

         \see didMakeActive
         */
        virtual void willMakeActive() noexcept {}

        /*!
         \brief Called after the controller has been made inactive.

         Either this callback or didCancelMakeInactive is guaranteed to be called if
         willMakeInactive returns normally.

         This function exists specifically so that mutexes can be locked in willMakeInactive that
         will remain locked throughout the transition. This function is where such mutexes would
         be unlocked. See the HSerial implementation for a demonstration of why this is necessary.

         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.

         \see willMakeInactive, didCancelMakeInactive
         */
        virtual void didMakeInactive() noexcept {}

        /*!
         \brief Called when the transition has been cancelled due to an improperly implemented 
         willMakeInactive.
         
         Either this callback or didMakeInactive is guaranteed to be called if
         willMakeInactive returns normally.

         This callback is never called unless there is a programmer error in the implementation 
         of a controller's willMakeInactive callback.
         If willMakeInactive returns normally then the transition will always occur _unless_
         that callback is incorrectly implemented (does not block access calls and wait for all
         calls to return). In that case
         the transition will be cancelled and a std::logic_error exception thrown. Before that
         occurs this didCancelMakeInactive callback will be called.
         
         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.

         \see willMakeInactive, didMakeInactive
         */
        virtual void didCancelMakeInactive() noexcept {}

        /*!
         \brief Called after willRemove if the transition has been cancelled.
         
         If willRemove() returns normally then either this callback or didRemove() will be called.
         
         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.
         
         \see willRemove, didRemove
         */
        virtual void didCancelRemove() noexcept {}

        /*!
         \brief Called after the controller has been removed from the access list.
         
         If willRemove() returns normally then either this callback or didCancelRemove() will be
         called.

         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.
         
         \see willRemove, didCancelRemove
         */
        virtual void didRemove() noexcept {}

        /*!
         \brief Called after the controller has been added to the access list.
         
         didAdd() is called before didMakeActive() for a controller that has been simultaneously
         added to the access list and made active.

         __Important__: No exceptions can be thrown from this function.

         The default implementation is empty.
         
         \see willRemove
         */
        virtual void didAdd() noexcept {}

        /*!
         \brief Called after the controller has been made active.
         
         An anticipated use of this callback is to apply the controller's preferred port settings
         to the serial port (e.g. baudrate), or to start up worker threads.
         
         Access functions are initially blocked when this callback is made (unless unblocked in
         didAdd, which is not recommended -- it will throw an exception -- since a controller
         can be added without being made active).
         
         The callback does not have to unblock the access calls. They will automatically be
         unblocked after this function returns or throws an error.

         Any exception thrown from this callback will propagate back to the calling function
         (e.g. makeActive()). However, even if an exception is thrown the controller will remain
         the active controller. This is reasonable because the code calling makeActive should
         have knowledge of the kinds of exceptions the controller can throw when it is made
         active.
         
         todo: Compare above with willMakeInactive, which can throw errors the makeActive
         caller cannot anticipate. Consider fix -- see notes at top of HSerialController.hpp.
         
         didAdd() is called before didMakeActive() for a controller that has been simultaneously
         added to the access list and made active.

         The default implementation is empty.
         
         \see willMakeInactive
         */
        virtual void didMakeActive() {}

        /// \} /Transition Callbacks


#pragma mark - Transition Utilities

        /*!
         \name Transition Utilities
         \anchor controller-transition-utilities

         These utility functions are used by the transition callbacks. They should not be called
         in any other cases.
         */
        /// \{

        /*!
         \brief Blocks calls to the access functions made outside of the transition thread.

         The blocking applies only to access calls made from other threads. Access calls made on the
         transition thread remain unblocked.

         Typically waitForAccessCallsToReturn() is used after this function to guarantee
         that the controller is done using the serial port.

         This function may be called only from a transition callback or subcall. Calling at any
         other time will cause an exception to be thrown.

         Does nothing if calls are already blocked.

         \throws std::logic_error Thrown if not called from a transition callback or subcall.
         \throws hserial::NotActiveController
         \see unblockAccessCalls, waitForAccessCallsToReturn
         */
        void blockAccessCalls();

        /*!
         \brief Unblocks calls to the access functions.

         Note that access function calls are automatically unblocked after a transition. This is
         true whether the transition completed normally or was cancelled by an exception.

         This function may be called only from a transition callback or subcall. Calling at any
         other time will cause an exception to be thrown.

         Does nothing if calls are already unblocked.

         \throws std::logic_error Thrown if not called from a transition callback or subcall.
         \throws hserial::NotActiveController
         \see blockAccessCalls, waitForAccessCallsToReturn
         */
        void unblockAccessCalls();

        /*!
         \brief Returns when all access calls have returned, or when the timeout is reached.

         Typically this function would be used after calling blockAccessCalls(). If access is not
         blocked then more access calls may be made even if this function returns successfully.

         This function may be called only from a transition callback or subcall. Calling at any
         other time will cause an exception to be thrown.

         \returns `true` if the function returned due to all access calls returning, `false` if the
         timeout was reached.

         \throws std::logic_error Thrown if not called from a transition callback or subcall.
         \throws hserial::NotActiveController
         \see blockAccessCalls, unblockAccessCalls
         */
        bool waitForAllAccessCallsToReturn(const std::chrono::milliseconds& timeout);
        
        /// \} /Transition Utilities


    private:

        friend class HSerialAccess;

#pragma mark - For Friends

        /*!
         \name For Friends
         
         Functions for use by friend classes (just HSerialAccess).
         */
        /// \{

        /*!
         \brief Returns the list of controllers associated with this controller.

         The controllers list consists of this controller and all delegate and subdelegate
         controllers. The list is sorted such that this controller is the first item (so the list
         is never empty), followed by the first degree delegates (this controller's delegates),
         then the second degree delegates, and so on.
         The order of delegates of the same degree is undefined.

         A controller might appear more than once in the list if a controller implementation
         decides to share delegates between controllers. This is allowed, but is probably bad
         design (callbacks will be called multiple times, for one thing).

         Used by HSerialAccess. If this controller is the current controller then this function
         returns the access list.
         */
        std::vector<HSerialController*> getControllersList();

        /*!
         \brief Indicates if a given controller is a delegate or subdelegate (of any degree).

         Used internally and by HSerialAccess.
         */
        bool hasAsDelegateOrSubdelegate(const HSerialController* otherController);

        /// \} /For Friends


#pragma mark - Internal Stuff

        /*!
         \name Internal Stuff
         
         Functions and members for internal use.
         */
        /// \{

        /*!
         \brief Indicates if a given controller is in the registered delegates list.
         
         Only checks first degree delegates, not subdelegates.
         
         Internal use only.
         */
        bool hasAsFirstDegreeDelegate(const HSerialController* otherController);

        /*!
         \brief Uses recursion to append all delegates of a given degree to the list.

         Degrees of delegation:
         - 0: this controller
         - 1: this controller's registered delegates
         - 2: the registered delegates of this controller's delegates
         - etc.

         Used by getControllersList.
         
         Internal use only.
         */
        size_t appendDelegatesOfDegree(std::vector<HSerialController*>& list, int degree);

        /*!
         \brief The controller's port instance.
         
         Keeping an instance of the port object ensures that the underlying HSerialDevice instance
         will not be destroyed. (Ownership of HSerialDevice instances is shared by port objects
         and the HSerialPortsManager.)
        
         Internal use only.
         */
        HSerialPort port;

        /*!
         \brief The serial access object.
         
         In addition to keeping a port instance the controller needs to keep an access object
         instance. (Ownership of access objects is shared by controllers -- the device object keeps
         only a weak reference.)
         
         Internal use only.
         */
        std::shared_ptr<HSerialAccess> access;

        /*!
         \brief The registered delegates of the controller.
         
         Specifically, these are the first degree delegates. Sub-delegates -- delegates of degree
         two and greater -- must be determined through recursion.
         
         Code that uses this list assumes that it is unchanging. In other words, delegates must be
         registered before the controller is used.
         
         Internal use only.
         */
        std::vector<HSerialController*> delegates;

        /// \} /Internal Stuff
    };
}


#endif /* HSerialController_hpp */
