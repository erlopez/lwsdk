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
#include "UDPSocketClient.h"

#include <unistd.h>
#include <netdb.h>


#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"


using namespace std;

namespace lwsdk
{

    UDPSocketClient::UDPSocketClient()
    {
        close();
    }


    UDPSocketClient::~UDPSocketClient()
    {
        close();
    }


    std::string UDPSocketClient::getLastError()
    {
        return lastError;
    }


    bool UDPSocketClient::isOpen() const
    {
        return sockfd != 0;
    }


    void UDPSocketClient::close()
    {
        lastError.clear();

        if ( sockfd )
            ::close( sockfd ); // global close()

        sockfd = 0;
    }


    bool UDPSocketClient::open( const std::string& remoteAddr, int remotePort )
    {
        close();

        struct addrinfo *addresses, *address;
        string strport = std::to_string( remotePort );

        struct addrinfo hints{0};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        int ret = getaddrinfo( remoteAddr.c_str(), strport.c_str(), &hints, &addresses );
        if ( ret != 0 )
        {
            lastError = "getaddrinfo() failed for " + remoteAddr + ":" + strport + " - " + gai_strerror( ret );
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        // Attempt to resolve hostname
        for( address = addresses; address != nullptr; address = address->ai_next )
        {
            if ((sockfd = socket( address->ai_family, address->ai_socktype, address->ai_protocol )) < 0 )
                continue;

            // Save address of opened socket and break
            sockAddr = *address->ai_addr;
            sockAddrLen = address->ai_addrlen;
            break;
        }

        freeaddrinfo( addresses );

        // Failed to resolve hostname?
        if ( address == nullptr )
        {
            lastError = "failed to create socket for " + remoteAddr + ":" + strport;
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        return true;
    }

    bool UDPSocketClient::send( void *data, int size )
    {
        lastError.clear();

        if ( !sockfd )
        {
            lastError = "socket is not open";
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        long ret = sendto( sockfd, data, size, 0, &sockAddr, sockAddrLen );
        if ( ret <= 0 )
        {
            lastError = "sendto() failed - "s + strerror( errno );
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        return true;
    }


} // ns

