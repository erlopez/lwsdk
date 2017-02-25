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
#include "NetlinkUEvent.h"

#include "Utils.h"
#include "Strings.h"

// Netlink
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/netlink.h>
#include <unistd.h>


#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"


using namespace std;

namespace lwsdk
{
    // Struct UEvent
    std::string UEvent::valueOf( const std::string &propName ) const
    {
        regex r( propName + "=([^\\n]+)" );
        smatch m;
        regex_search( data, m, r );
        
        return m.empty() ? "" : m[1].str();
    }
 
    long UEvent::intValueOf( const std::string &propName, long defValue ) const
    {
        return Strings::parseLong( valueOf(propName), defValue );
    }


    // Class NetlinkUEvent
    NetlinkUEvent::NetlinkUEvent( const UEventCallback_t&  uEventCallback )
    {
        this->uEventCallback = uEventCallback;

        if ( uEventCallback == nullptr )
            throw RuntimeException( string(__PRETTY_FUNCTION__ ) + " - uEventCallback can't be null." );

        // Start reader thread
        keepWorking = true;
        uEventReaderThread = new thread( &NetlinkUEvent::readerThread, this );
    }

    
    NetlinkUEvent::~NetlinkUEvent()
    {
        // End reader thread
        if ( uEventReaderThread != nullptr )
        {
            keepWorking = false;          // signal reader thread to exit
            uEventReaderThread->join();   // Wait thread exit
            delete uEventReaderThread;    // clean up
            uEventReaderThread = nullptr;
        }

    }

 

    void NetlinkUEvent::readerThread()
    {
        while ( keepWorking )
        {
            sockaddr_nl srcAddr{0};
            int socketfd;
            int ret;

            // 1. Configure netlink socket to subscribe to kernel uevent stream
            srcAddr.nl_family = AF_NETLINK;
            srcAddr.nl_pid = (gettid() << 16) + getpid();  // Port ID
            srcAddr.nl_groups = -1;

            socketfd = socket( AF_NETLINK, (SOCK_DGRAM | SOCK_NONBLOCK), NETLINK_KOBJECT_UEVENT );
            if ( socketfd < 0 )
            {
                loge( "Netlink reader thread: failed to create netlink socket, could not start." );
                return;
            }

            ret = bind( socketfd, (sockaddr *) &srcAddr, sizeof( srcAddr ));
            if ( ret )
            {
                loge( "Netlink reader thread: failed to bind netlink socket, could not start." );
                close( socketfd );
                return;
            }


            // 2. Configure epoll on the socket to allow reads with timeouts
            #define MAX_EPOLL_EVENTS 1
            epoll_event events[MAX_EPOLL_EVENTS];

            // Create epoll instance
            int epollfd = epoll_create( 1 );  // tip: "1" size argument is ignored, but must be > 0
            if ( epollfd < 0 )
            {
                loge( "Netlink reader thread: failed to create epoll instance, could not start." );
                close( socketfd );
                return;
            }

            // Add socketfd to the watch list
            epoll_event event{0};
            event.events = EPOLLIN;         // Can append "|EPOLLOUT" for write events as well
            event.data.fd = socketfd;

            ret = epoll_ctl( epollfd, EPOLL_CTL_ADD, socketfd, &event );
            if ( ret < 0 )
            {
                loge( "Netlink reader thread: failed to add socket to epoll instance, could not start." );
                close( epollfd );
                close( socketfd );
                return;
            }

            // 3. Loop to wait for incoming data
            logi( "Netlink reader thread started ..." );

            while ( keepWorking )
            {
                #if LOGGER_ENABLED
                printf( "o" );
                fflush( stdout );
                #endif

                // Wait for socket data available
                int nReady = epoll_wait( epollfd, events, MAX_EPOLL_EVENTS, 500 /*timeout ms*/ );

                if ( nReady < 0 )
                {
                    // This usually happens after PC wake from sleep
                    loge( "Netlink reader thread: epoll_wait() error on netlink socket (errno=%i %s); attempting to restart thread ...",
                          errno, strerror( errno ));
                    break;
                }
                else if ( nReady == 0 || !(events[0].events & EPOLLIN))
                {
                    continue;
                }

                // Read socket data
                long len = recv( socketfd, buf, sizeof( buf ), 0 );
                if ( len < 0 && errno != EAGAIN )
                {
                    logw( "Netlink reader thread: read() error on netlink socket (errno=%i %s)",
                          errno, strerror( errno ));
                    this_thread::sleep_for( 1s );
                }
                else if ( len > 0 )
                {
                    logY( "------------------------ UMessage len=%ld ----------------------", len );

                    // Ignore libudev messages
                    if ( strcmp( "libudev", buf ) == 0 )
                        continue;


                    #if LOGGER_ENABLED
                    Utils::memdump( buf, len );
                    #endif

                    // Convert all null-terminatord in message to LF \n
                    for ( int i = 0; i < len; i++ )
                        if ( buf[i] == 0 )
                            buf[i] = '\n';

                    // Call user defined callback
                    try
                    {
                        uEventCallback( UEvent( string( buf, len )));
                    }
                    catch ( const std::exception &e )
                    {
                        loge( "User's uEventCallback() finished with errors: %s", e.what());
                    }

                }


            }   //loop


            // Close socket
            close( epollfd );
            close( socketfd );

            logi( "Netlink reader thread ended." );

        }  /*outer while*/

    }



} // ns

