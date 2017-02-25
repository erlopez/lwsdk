/********************************************************************************
 *  This file is part of the lopezworks SDK utility library for C/C++ (lwsdk).
 *
 *  Copyright (C) 2015-2017 Edwin R. Lopez
 *  http://www.lopezworks.info
 *  https://github.com/erlopez/lwsdk
 *
 *  This source code is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation LGPL v2.1
 *  (http://www.gnu.org/licenses/).
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301  USA.
 ********************************************************************************/
#ifndef SERIALPORT_H
#define SERIALPORT_H


#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <condition_variable>

#include <termios.h>  // for baud rate constants B115200, B921600, etc..

namespace lwsdk
{
    /**
     * User callback to receive serial port open/close events.
     * @param portName    Port name the text line originates from
     * @param isOpen      Boolean indicating whether or not the serial port is open.
     *                    When connecting to a serial port created by a
     *                    USB-to-Serial cable, a serial port close event is generated
     *                    if the user pulls the cable out.
     */
    typedef std::function<void( const std::string &portName, bool isOpen )>
            SPStatusCallback_t;

    /**
     * User callback to receive lines of text send over serial port
     * @param portName  Port name the text line originates from
     * @param line      The received text line
     */
    typedef std::function<void( const std::string &portName, const std::string &line )>
            SPLineCallback_t;

    /**
     * User callback to receive characters of received over serial port
     * @param portName  Port name the data originates from
     * @param data      Characters received
     * @param len       Number of characters received
     */
    typedef std::function<void( const std::string &portName, const char *data, uint32_t len )>
            SPDataCallback_t;


    class SerialPort
    {
        int portfd{-1};
        int baudRate{115200};
        bool useParity{false};
        bool use2StopBits{false};
        bool useFlowControl{false};
        uint32_t interCharacterWriteDelay{0};
        std::string portName{"/dev/ttyUSB0"};
        char lastError[255]{0};

        SPLineCallback_t   lineCallback{nullptr};
        SPDataCallback_t   dataCallback{nullptr};
        SPStatusCallback_t statusCallback{nullptr};

        std::atomic_bool  keepWorking{false};
        std::atomic_bool  isConnected{false};
        std::thread *portReaderThread{nullptr};
        std::condition_variable idleCondVar;
        
        void readerThread();
        
    public:
        SerialPort();

        virtual ~SerialPort();

        /**
         * Configure serial port parameters.
         *
         * @param portName        Port name, eg. /dev/ttyUSB0, /dev/ttyACM0, etc.
         * @param baudRate        Set the transfer speed: B115200, B921600 etc.
         * @param useFlowControl  Specifies whether or not to enable HW flow control
         * @param useParity       Specifies whether or not to enable parity checking
         * @param use2StopBits    False uses 1-stop-bit, true uses 2-stop-bits
         */
        void setConfig( const std::string &portName, uint32_t baudRate, bool useFlowControl = false,
                        bool useParity = false, bool use2StopBits = false );

        /**
         * Adds a small delay between each character being written out.
         * The PC is much faster than a microcontroller; writing data at full
         * speed may overwhelm a MCU. In such cases, adding a delay between the
         * characters being written may give the MCU time to catch up with
         * reading and processing the incoming data.
         *
         * By default interCharacterWriteDelay is 0 and might be increased on
         * per case basis.  
         *
         * @param writeInterCharDelayUs  Delay between characters being written
         *                               out in microseconds (us).
         */
        void setInterCharacterWriteDelay( uint32_t interCharacterWriteDelay );

        /**
         * Returns a multiline string with the current port configuration.
         */
        std::string getConfig();

        /**
         * Test if the last operation ended up with errors.
         */
        bool hasErrors();

        /**
         * Returns the last operation error information (if any); blank if
         * no errors occurred.
         */
        std::string getErrors();

        /**
         * Clear any outstanding errors.
         */
        void clearErrors();

        /**
         * Set user-defined callback to receive serial port open/close events.
         * @param statusCallback  Pointer to user-defined function. Set to null
         *                        to remove any previously set callback.
         */
        void setStatusCallback( const SPStatusCallback_t& statusCallback );

        /**
         * Set user-defined callback to receive raw serial port data.
         * @param dataCallback  Pointer to user-defined function. Set to null
         *                      to remove any previously set callback.
         */
        void setDataCallback( const SPDataCallback_t& dataCallback );

        /**
         * Set user-defined callback to receive line-delimited serial port text.
         * @param dataCallback  Pointer to user-defined function. Set to null
         *                      to remove any previously set callback.
         */
        void setLineCallback( const SPLineCallback_t& lineCallback );

        /**
         * Opens the serial port with the current configuration.
         * @return true on success, false otherwise; use getError() to determine the cause.
         */
        bool open();

        /**
         * Closes the serial port. Does nothing if the port is not open.
         * @return true on success, false otherwise; use getError() to determine the cause.
         */
        bool close();

        /**
         * Test if the serial port is open for communications .
         */
        bool isOpen();

        /**
         * Writes text to the serial port.
         * @param text Text to send.
         * @return The number of bytes sent. Returns -1 on error.
         */
        int write( const std::string &text );

        /**
         * Writes raw data to the serial port.
         * @param data Data to send out.
         * @param len  Length of the data in bytes.
         * @return The number of bytes sent. Returns -1 on error.
         */
        int write( const uint8_t *data, int len );
    };

}
#endif //SERIALPORT_H
