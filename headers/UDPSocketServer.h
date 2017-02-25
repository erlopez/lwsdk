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
#ifndef UDP_SOCKET_SERVER_H
#define UDP_SOCKET_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include "Exceptions.h"
#include <net/if.h>

namespace lwsdk
{
    // Size of the buffer to receive network data
    #define UDP_SOCKET_SERVER_BUF_SZ_8K 8192

    /**
     * User callback to receive UDP packets
     * @param data        Received data
     * @param size        Data size in bytes
     * @param srcAddr     IP Address of the sender
     * @param userData    User-defined data associated with this UDP server instance
     */
    typedef std::function<void ( const uint8_t *data, uint32_t size, const std::string &srcAddr,
                                 void *userData )> UDPCallback_t;


    /**
     * Easy socket server implementation for receiving UDP network packets
     */
    class UDPSocketServer
    {
        UDPCallback_t           udpCallback{nullptr};
        void*                   userData{nullptr};
        std::atomic_bool        keepWorking{false};
        std::thread             *udpReaderThread{nullptr};
        std::condition_variable idleCondVar;
        std::atomic_bool        isConnected{false};
        int                     port{0};
        int                     sockfd{0};
        uint32_t                maxBufSize{0};
        std::string             lastError{};
        std::string             ifname{};

        void readerThread();

    public:
        /**
         * Creates a UDP socket server instance
         * @param udpCallback
         *
         * @param port          Port to listen for incoming data
         * @param udpCallback   User-defined function to receive event notifications
         * @param userData      Optional user-defined data passed to callback
         * @param maxBufSize    Optional maximum packet size expected. Packets larger
         *                      that this value are silently truncated. The default is 8 KB.
         * @param ifname        Optional: bind this socket to a particular device like “eth0”,
         *                      set to blank "" to listen on all interfaces. This
         *                      value cannot exceed more than IF_NAMESIZE characters.
         */
        explicit UDPSocketServer( int port, const UDPCallback_t& udpCallback, void *userData = nullptr,
                                  uint32_t maxBufSize = UDP_SOCKET_SERVER_BUF_SZ_8K,
                                  const std::string& ifname = "" );

        virtual ~UDPSocketServer();

        /**
         * Starts the UDP socket server.
         * @return true on success, false otherwise.  See getLastError().
         */
        bool start();

        /**
         * Stops the UDP socket server.
         */
        void stop();

        /**
         * Returns true if the server is running, false otherwise.
         */
        bool isRunning() { return isConnected; }

        /**
         * Returns the last operation error information (if any); blank if
         * no errors occurred.
         */
        std::string getLastError();

    };

}
#endif //UDP_SOCKET_SERVER_H
