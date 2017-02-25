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
#include "UDPSocketServer.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>


#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"


using namespace std;

namespace lwsdk
{

    UDPSocketServer::UDPSocketServer( int port, const UDPCallback_t&  udpCallback, void *userData,
                                      uint32_t maxBufSize, const std::string& ifname )
    {
        this->udpCallback = udpCallback;
        this->userData = userData;
        this->port = port;
        this->ifname = ifname;
        this->maxBufSize = maxBufSize;

        if ( udpCallback == nullptr )
            throw RuntimeException( string(__PRETTY_FUNCTION__ ) + " - udpCallback can't be null." );

        if ( ifname.length() > IF_NAMESIZE )
            throw RuntimeException( string(__PRETTY_FUNCTION__ ) + " - ifname='" + ifname + "' exceeds the " +
                                      std::to_string(IF_NAMESIZE) + " characters maximum"  );

        // Start reader thread
        keepWorking = true;
        udpReaderThread = new thread( &UDPSocketServer::readerThread, this );
    }

    
    UDPSocketServer::~UDPSocketServer()
    {
        // End reader thread
        if ( udpReaderThread != nullptr )
        {
            keepWorking = false;       // signal reader thread to exit
            idleCondVar.notify_one();  // wake thread if sleeping
            udpReaderThread->join();   // Wait thread exit
            delete udpReaderThread;    // clean up
            udpReaderThread = nullptr;
        }

        stop();
    }


    std::string UDPSocketServer::getLastError()
    {
        return lastError;
    }


    void UDPSocketServer::stop()
    {
        isConnected = false;
        lastError.clear();

        if ( sockfd )
        {
            shutdown( sockfd, SHUT_RDWR ); // unblock socket waiting in recvfrom()
            close( sockfd );
        }

        sockfd = 0;
    }

    bool UDPSocketServer::start()
    {
        int ret;
        
        stop(); // close previous connection if any
        lastError.clear();

        // Create UDP socket
        sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if ( sockfd < 0 )
        {
            lastError = "socket() failed - "s + strerror( errno );
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        // Bind this socket to a particular device like “eth0”, if any
        if ( !ifname.empty() )
        {
            ret = setsockopt( sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname.c_str(), ifname.length() );
            if ( ret < 0 )
            {
                string errmsg = strerror( errno );
                stop();
                lastError = "setsockopt() failed - " + errmsg;
                loge( "Error: %s", lastError.c_str() );
                return false;
            }

        }


        // Bind to port
        sockaddr_in sockAddr{0};
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = htons( port );
        sockAddr.sin_addr.s_addr = INADDR_ANY;

        socklen_t sockLen = sizeof( sockAddr );

        ret = bind( sockfd, (sockaddr *) &sockAddr, sockLen );
        if ( ret < 0 )
        {
            string errmsg = strerror( errno );
            stop();
            lastError = "bind() failed - " + errmsg;
            loge( "Error: %s", lastError.c_str() );
            return false;
        }

        isConnected = true;
        idleCondVar.notify_one(); // wake idle thread
        return true;
    }

    void UDPSocketServer::readerThread()
    {
        sockaddr_storage storageAddr{};
        socklen_t sockLen;
        string    srcAddr;
        auto      buf = std::make_unique<uint8_t[]>( maxBufSize );

        logw( "UDPSocketServer reader thread started." );

        // Thread Main loop
        while ( keepWorking )
        {
            // If not connected, sleep until timeout or waken by event
            if ( !isConnected )
            {
                mutex mtx;
                unique_lock lock( mtx );
                idleCondVar.wait_for( lock, 10s, [this]() { return !this->keepWorking || this->isConnected; } );
            }

            if ( !keepWorking )
                break;

            // Block here, read data
            sockLen = sizeof(storageAddr);
            long count = recvfrom( sockfd, buf.get(), maxBufSize, 0, (sockaddr*)&storageAddr, &sockLen );
            if ( !isConnected )
            {
                logw("UDPSocketServer reader thread: error recvfrom() disconnected.");
                continue;
            }
            else if ( count <= 0  )
            {

                loge("UDPSocketServer reader thread: error recvfrom() - %s.", strerror(errno) );
                continue;
            }

            // Extract source IP address
            if ( storageAddr.ss_family == AF_INET )
            {
                uint32_t ip = ((sockaddr_in*)&storageAddr)->sin_addr.s_addr;
                char ipaddr[INET_ADDRSTRLEN]{0}; // fits ipv4

                inet_ntop( AF_INET, &ip, ipaddr, sizeof(ipaddr) ); // IP addr
                srcAddr = ipaddr;
            }
            else
            {
                continue;
            }


            // Call user defined callback
            try
            {
                udpCallback( buf.get(), count, srcAddr, userData );
            }
            catch ( const std::exception &e )
            {
                loge( "UDPSocketServer reader thread: user's udpCallback() finished with errors: %s", e.what());
            }


        } /*while*/

        keepWorking = false;
        logw( "UDPSocketServer reader thread exited."  );

    }



} // ns

