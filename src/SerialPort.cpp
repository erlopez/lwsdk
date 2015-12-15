/********************************************************************************
 *  This file is part of the lopezworks SDK utility library for C/C++ (lwsdk).
 *
 *  Copyright (C) 2015-2017 Edwin R. Lopez
 *  http://www.lopezworks.info
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
#include <map>
#include <sstream>
#include <utility>
#include <mutex>
#include <thread>
#include <optional>


// serial port
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string>


#include "SerialPort.h"
#include "Files.h"
#include "Exceptions.h"
#include "Utils.h"
#include "ConcurrentQueue.h"

#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"


using namespace std;

namespace lwsdk
{

    SerialPort::SerialPort()
    {
        // Start reader thread
        keepWorking = true;
        isConnected = false;
        portReaderThread = new thread( &SerialPort::readerThread, this );
    }

    SerialPort::~SerialPort()
    {
        // End reader thread
        if ( portReaderThread != nullptr )
        {
            keepWorking = false;        // signal reader thread to exit
            portReaderThread->join();   // Wait thread exit
            delete portReaderThread;    // clean up
            portReaderThread = nullptr;
        }

        // Close port
        close();
    }

    void SerialPort::setConfig( const std::string &portName, uint32_t baudRate, bool useFlowControl,
                                bool useParity, bool use2StopBits )
    {
        this->portName = portName;
        this->baudRate = (int)baudRate;
        this->useFlowControl = useFlowControl;
        this->useParity = useParity;
        this->use2StopBits = use2StopBits;
    }


    void SerialPort::setInterCharacterWriteDelay( uint32_t interCharacterWriteDelay )
    {
        this->interCharacterWriteDelay = interCharacterWriteDelay;

    }

    std::string SerialPort::getConfig()
    {
        ostringstream ss;

        ss << "Port:                     " << portName << endl;
        ss << "BaudRate:                 " << baudRate << endl;
        ss << "UseParity:                " << (useParity ? "true" : "false") << endl;
        ss << "Use2StopBits:             " << (use2StopBits ? "true" : "false") << endl;
        ss << "UseFlowControl:           " << (useFlowControl ? "true" : "false") << endl;
        ss << "DataBits:                 8" << endl;
        ss << "interCharacterWriteDelay: " << interCharacterWriteDelay << "us" << endl;

        return ss.str();
    }

    bool SerialPort::hasErrors()
    {
        return lastError[0] != 0;
    }

    std::string SerialPort::getErrors()
    {
        return lastError;
    }

    void SerialPort::clearErrors()
    {
        lastError[0] = 0;
    }

    void SerialPort::setStatusCallback( const SPStatusCallback_t& statusCallback )
    {
        this->statusCallback = statusCallback;
    }

    void SerialPort::setDataCallback( const SPDataCallback_t& dataCallback )
    {
        this->dataCallback = dataCallback;
    }


    void SerialPort::setLineCallback( const SPLineCallback_t& lineCallback )
    {
        this->lineCallback = lineCallback;
    }


    bool SerialPort::isOpen()
    {
        return isConnected;
    }


    uint32_t SerialPort::write( const std::string& text )
    {
        return write( (uint8_t*)text.data(), (uint32_t) text.length() );
    }


    uint32_t SerialPort::write( const uint8_t *data, uint32_t len  )
    {
        uint32_t count = 0;
        uint32_t bytesWritten;


        #if LOGGER_ENABLED
        printf( FG14 );
        Utils::memdump( data, len );
        printf( NOC );
        #endif

        while ( count < len && isConnected )
        {
            if ( interCharacterWriteDelay > 0 )
            {
                // insert inter-character delay, to not overwhelm device
                bytesWritten = ::write( portfd, &data[count], 1 );
                usleep( interCharacterWriteDelay );
            }
            else
            {
                bytesWritten = ::write( portfd, &data[count], len - count );
            }

            count += bytesWritten;
            if ( bytesWritten < 0 )
            {
                snprintf( lastError, sizeof( lastError ), "Error while writing to %s (errno=%i %s)",
                          portName.c_str(), errno, strerror(errno) );
                logw( "%s", lastError );
                return -1;
            }
            
            //logi( "Wrote to %s (%d of %d) bytes", portName.c_str(), count, len );
        } // while

        return count;
    }

    
    bool SerialPort::close()
    {
        int ret = 0;
        clearErrors();


        // Close port
        if ( portfd > 0 )
        {
            ret = ::close( portfd );
            if ( ret != 0 )
            {
                snprintf( lastError, sizeof( lastError ), "Unable to close %s (errno=%i %s)",
                          portName.c_str(), errno, strerror(errno) );
                logw( "%s", lastError );
                return false;
            }
            portfd = -1;
        }

        // Put read thread into standby
        if ( isConnected )
        {
            logi( "Closed serial port %s", portName.c_str() );
            isConnected = false;

            if ( statusCallback )
                statusCallback( portName, isConnected );
        }

        return true;
    }


    bool SerialPort::open()
    {
        close();
        clearErrors();

        // Open port
        portfd = ::open( portName.c_str(), O_RDWR );

        if ( portfd < 0 )
        {
            snprintf( lastError, sizeof( lastError ), "Unable to open %s (errno=%i, %s)",
                      portName.c_str(), errno, strerror(errno));
            logw( "%s", lastError );

            return false;
        }


        // Read in existing settings, and handle any error
        struct termios tty{};

        if ( tcgetattr( portfd, &tty ) != 0 )
        {
            close();

            snprintf( lastError, sizeof( lastError ), "Failed to read %s config via tcgetattr() - errno=%i, %s",
                      portName.c_str(), errno, strerror(errno));
            logw( "%s", lastError );

            return false;
        }

        //
        // Config serial port
        // See brief https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
        //

        // Parity
        if ( useParity )
            tty.c_cflag |= PARENB;  // Set useParity bit, enabling useParity
        else
            tty.c_cflag &= ~PARENB; // Clear useParity bit, disabling useParity (most common)

        // Stop bits
        if ( use2StopBits )
            tty.c_cflag |= CSTOPB;  // Set stop field, two stop bits used in communication
        else
            tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)

        // Hardware Flow Control
        if ( useFlowControl )
            tty.c_cflag |= CRTSCTS;  // Enable RTS/CTS hardware flow control
        else
            tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)


        // Data bits per byte
        tty.c_cflag &= ~CSIZE;       // Clear all the size bits, then use one of the statements below
        tty.c_cflag |= CS8;          // 8 bits per byte (most common);  CS5 .. CS8

        // Disables signal line carrier detect, prevents process death from SIGHUP when a modem disconnects: CLOCAL
        // Enable read data: CREAD
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

        // Disabling Canonical Mode to receive data on char basis rather than line-by-line
        tty.c_lflag &= ~ICANON;

        // Disable echoing back of sent characters
        tty.c_lflag &= ~ECHO;        // Disable echo
        tty.c_lflag &= ~ECHOE;       // Disable erasure
        tty.c_lflag &= ~ECHONL;      // Disable new-line echo

        // Disables software flow control
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl

        // Disable character interpretation
        tty.c_lflag &= ~ISIG;        // Disable interpretation of INTR, QUIT and SUSP
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes
        tty.c_oflag &= ~OPOST;       // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= ~ONLCR;       // Prevent conversion of newline to carriage return/line feed

        // Specify read() timeout via VMIN and VTIME
        // Meanings:
        //   VMIN = 0, VTIME = 0: No blocking, return immediately with what is available
        //   VMIN > 0, VTIME = 0: This will make read() always wait for bytes (exactly how many is
        //                        determined by VMIN), so read() could block indefinitely.
        //   VMIN = 0, VTIME > 0: This is a blocking read of any number of chars with a maximum
        //                        timeout (given by VTIME). read() will block until either any
        //                        amount of data is available, or the timeout occurs.
        //   VMIN > 0, VTIME > 0: Block until either VMIN characters have been received, or VTIME after
        //                        first character has elapsed. Note that the timeout for VTIME does not
        //                        begin until the first character is received.
        //
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 5;      // Wait for up to 0.5s (5 deciseconds), returning as soon as any data is received.

        // Baud rate
        cfsetspeed( &tty, baudRate ); // allegedly, custom values work only on GNUC, otherwise use enums like B9600, etc.


        // Apply tty settings, also checking for error
        if ( tcsetattr( portfd, TCSANOW, &tty ) != 0 )
        {
            close();

            snprintf( lastError, sizeof( lastError ), "Failed to write %s config via tcsetattr() - errno=%i, %s",
                      portName.c_str(), errno, strerror(errno));
            logw( "%s", lastError );

            return false;
        }

        // Tell reader thread port is ready
        isConnected = true;

        logi( "Opened serial port %s", portName.c_str());

        // Notify user callback if any
        if ( statusCallback )
            statusCallback( portName, isConnected );


        return true;

    }

    void SerialPort::readerThread()
    {
        bool lastConnectionState = true;
        struct stat st{};
        char buf[255];
        size_t len;
        string line;


        logi( "Reader thread started on serial port %s", portName.c_str());

        //{
        //    std::lock_guard<std::mutex> lock(mtx);
        //}

        while ( keepWorking )
        {

            if ( lastConnectionState != isConnected )
            {
                lastConnectionState = isConnected;
                logi( "Reader thread on serial port %s is %s", portName.c_str(),
                       isConnected ? "ACTIVE" : "STANDBY" );
            }

            // If not connected, do short sleeps while standing by for port getting open
            if ( !isConnected )
            {
                std::this_thread::sleep_for( chrono::milliseconds(500) );
                continue;
            }

            #if LOGGER_ENABLED
            printf( "o" );
            fflush( stdout );
            #endif

            len = read( portfd, buf, sizeof(buf) ); // blocks until VTIME timeout

            if ( len < 0 || errno != 0 )
            {
                snprintf( lastError, sizeof( lastError ), "Failed to read from %s - errno=%i, %s",
                          portName.c_str(), errno, strerror(errno));

                logw( "%s", lastError );

                close();
            }
            else if (len > 0)
            {
                // call user defined data callback -- if any
                if ( dataCallback )
                {
                    try
                    {
                        dataCallback( portName, buf, len );
                    }
                    catch ( const std::exception& e )
                    {
                        loge( "User's dataCallback() finished with errors: %s", e.what() );
                    }
                }

                #if LOGGER_ENABLED
                Utils::memdump( buf, len );
                #endif

                // call user defined line callback -- if any
                if ( lineCallback )
                {

                    line.append( buf, len );
                    string s;

                    while ( !(s = Strings::findMatch(line,"[^\r\n]*(\r\n|\r|\n)")).empty() )
                    {      
                        try
                        {
                            line.erase(0, s.length() );
                            s = Strings::replaceAll(s, "[\r\n]+$", "");
                            lineCallback( portName, s );
                        }
                        catch ( const std::exception& e )
                        {
                            loge( "User's dataCallback() finished with errors: %s", e.what() );
                        }
                    } //while
                } 


            }
            else
            {
                // here len=0 means either a read() timeout occurred
                // or that the /dev/ttyXXXX device was removed
                // Check the stat.st_nlink to see if it is still >= 1
                fstat( portfd, &st );

                if ( st.st_nlink == 0 )
                {
                    // This is likely to occur when the USB-to-serial cable is unplugged
                    snprintf( lastError, sizeof( lastError ), "No longer detecting serial port %s",
                              portName.c_str() );

                    logw( "%s", lastError );

                    close();
                }
            }

        }


        logi( "Reader thread ended on serial port %s", portName.c_str());

    }


} // ns

