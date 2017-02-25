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
#ifndef UDP_SOCKET_CLIENT_H
#define UDP_SOCKET_CLIENT_H

#include <string>
#include <atomic>
#include "Exceptions.h"
#include <sys/socket.h>

namespace lwsdk
{
    /**
     * Easy socket client implementation for sending UDP network packets
     */
    class UDPSocketClient
    {
        int                 sockfd{0};
        std::string         lastError{};
        sockaddr            sockAddr{0};
        socklen_t           sockAddrLen{0};

    public:
        /**
         * Create UDP socket client instance
         */
        explicit UDPSocketClient();

        virtual ~UDPSocketClient();

        /**
         * Open UDP socket to send data to a remote address and port.
         * Any previous opened address and port is closed before the
         * new connection is opened.
         * @return true on success, false otherwise. See getLastError().
         */
        bool open( const std::string& remoteAddr, int remotePort );

        /**
         * Closes the UDP socket. This function is called automatically by the
         * destructor when object goes out of scope.
         */
        void close();

        /**
         * Test if the connection is open.
         */
        bool isOpen() const;

        /**
         * Send UDP packets to remote endpoint.
         * @param data  Pointer to data being sent on UDP packet
         * @param size  Size of the data in bytes
         * @return
         */
        bool send( void *data, int size );

        /**
         * Returns the last operation error information (if any); blank if
         * no errors occurred.
         */
        std::string getLastError();

    };

}
#endif //UDP_SOCKET_CLIENT_H
