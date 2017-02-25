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
#include "NetlinkRoute.h"

#include <cstdio>

// Netlink
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"
#include "Utils.h"


using namespace std;

namespace lwsdk
{
    // Struct Route
    std::string RouteEvent::toString() const
    {
        char buf[256]{0};
        string stype;

        switch ( type )
        {
            case ROUTE_EVENT_NEWROUTE: stype = "ROUTE_EVENT_NEWROUTE"; break;
            case ROUTE_EVENT_DELROUTE: stype = "ROUTE_EVENT_DELROUTE"; break;
            case ROUTE_EVENT_NEWADDR:  stype = "ROUTE_EVENT_NEWADDR"; break;
            case ROUTE_EVENT_DELADDR:  stype = "ROUTE_EVENT_DELADDR"; break;
            case ROUTE_EVENT_NEWLINK:  stype = "ROUTE_EVENT_NEWLINK"; break;
            case ROUTE_EVENT_DELLINK:  stype = "ROUTE_EVENT_DELLINK"; break;
            case ROUTE_EVENT_NONE:     stype = "ROUTE_EVENT_NONE"; break;
            default:                   stype = "ROUTE_EVENT_??? (UNKNOWN)"; break;
        }

        snprintf( buf, sizeof(buf), "RouteEvent{ type='%s', ifname='%s', ipaddr='%s', isUp='%s', isRunning='%s' }",
                 stype.c_str(), ifname.c_str(), ipaddr.c_str(), isUp ? "true" : "false",  isRunning ? "true" : "false" );

        
        return buf;
    }


    // Class NetlinkRoute
    NetlinkRoute::NetlinkRoute( const RouteEventCallback_t&  routeCallback, const std::string& netns )
    {
        this->routeCallback = routeCallback;
        this->netns = netns;

        if ( routeCallback == nullptr )
            throw RuntimeException( string(__PRETTY_FUNCTION__ ) + " - routeCallback can't be null." );

        // Start reader thread
        keepWorking = true;
        routeReaderThread = new thread( &NetlinkRoute::readerThread, this );
    }

    
    NetlinkRoute::~NetlinkRoute()
    {
        // End reader thread
        if ( routeReaderThread != nullptr )
        {
            keepWorking = false;          // signal reader thread to exit
            routeReaderThread->join();   // Wait thread exit
            delete routeReaderThread;    // clean up
            routeReaderThread = nullptr;
        }

    }

    static RouteEvent toRouteEvent( const nlmsghdr *msgHdr )
    {
        RouteEvent evt{};
        evt.type = ROUTE_EVENT_NONE;

        // Get information of network interface
        ifinfomsg *info = (ifinfomsg *) NLMSG_DATA( msgHdr );

        // Collect attributes
        rtattr *attributes[IFA_MAX + 1] {nullptr};
        rtattr *attribute = IFLA_RTA( info );
        uint32_t len = msgHdr->nlmsg_len;

        while ( RTA_OK( attribute, len))
        {
            if ( attribute->rta_type <= IFA_MAX )
                attributes[attribute->rta_type] = attribute;

            attribute = RTA_NEXT( attribute, len ); // move to next attribute
        }

        // Check for routing table change events
        int type = msgHdr->nlmsg_type;

        if ( type == RTM_NEWROUTE )
        {
            logC("ROUTE_EVENT_NEWROUTE");
            evt.type = ROUTE_EVENT_NEWROUTE;
            return evt;
        }
        else if ( type == RTM_DELROUTE )
        {
            logC("ROUTE_EVENT_DELROUTE");
            evt.type = ROUTE_EVENT_DELROUTE;
            return evt;
        }

        // Ignore messages we don't know how to handle
        if ( type!=RTM_DELADDR && type!=RTM_NEWADDR && type!=RTM_DELLINK && type!=RTM_NEWLINK )
        {
            logY("Unsupported message %d", type );
            return evt;
        }

        // No interface name?, bail here
        if ( !attributes[IFLA_IFNAME] )
        {
            logY("No interface name found, bailing!");
            return evt;
        }

        // No interface name?, bail here
        if ( !attributes[IFLA_IFNAME] )
        {
            logY("No interface name found, bailing!");
            return evt;
        }

        evt.ifname = (char *) RTA_DATA( attributes[IFLA_IFNAME] ); // get network interface name
        evt.isUp = info->ifi_flags & IFF_UP;
        evt.isRunning = info->ifi_flags & IFF_RUNNING;

        logC("Found interface name: '%s' %s %s", evt.ifname.c_str(), evt.isUp ? "UP" : "DOWN",  evt.isRunning ? "CARRIER" : "NO CARRIER" );

        if ( attributes[IFA_LOCAL] )
        {
            char ipaddr[INET6_ADDRSTRLEN]{0}; // fits both ipv4 and ipv6   IPs
            inet_ntop( AF_INET, RTA_DATA(attributes[IFA_LOCAL]), ipaddr, sizeof(ipaddr) ); // IP addr
            evt.ipaddr = ipaddr;
        }

        switch ( msgHdr->nlmsg_type )
        {
            case RTM_DELADDR:
                logC("ROUTE_EVENT_DELADDR");
                evt.type = ROUTE_EVENT_DELADDR;
                break;

            case RTM_DELLINK:
                logC("ROUTE_EVENT_DELLINK");
                evt.type = ROUTE_EVENT_DELLINK;
                break;

            case RTM_NEWLINK:
                logC("ROUTE_EVENT_NEWLINK");
                evt.type = ROUTE_EVENT_NEWLINK;
                break;

            case RTM_NEWADDR:
                logC("ROUTE_EVENT_NEWADDR");
                evt.type = ROUTE_EVENT_NEWADDR;
                break;
        }

        return evt;
    }


    void NetlinkRoute::readerThread()
    {
        // Set network namespace for this thread (if any)
        if ( !netns.empty() )
        {
            logB("Attempting to switch thread to network namespace to '%s' ... ", netns.c_str() );
            logB("Current program permissions: real-user-id=%d, effective-user-id=%d (from executable's setuid bit)",
                  getuid(), geteuid() );
            
            string nspath = "/var/run/netns/" + netns;
            
            int fd = open( nspath.c_str(), O_RDONLY );
            if ( fd < 0 )
            {                      
                loge("Network namespace '%s' not found. Event monitoring could not be started.", netns.c_str() );
                keepWorking = false;
                return;                
            }

            int ret = setns( fd, CLONE_NEWNET );
            close( fd );

            if ( ret < 0 )
            {
                loge("setns() failed to switch network namespace '%s'. ", netns.c_str() );
                loge("Are you root or is the executable program owned by root AND has the setuid bit set via `chmod u+s /path/to/executable`?" );
                loge("Event monitoring could not be started." );
                keepWorking = false;
                return;
            }
        }

        while ( keepWorking )
        {
            sockaddr_nl srcAddr{0};
            int socketfd;
            int ret;

            // 1. Configure Netlink route socket to subscribe to kernel route stream
            srcAddr.nl_family = AF_NETLINK;
            srcAddr.nl_pid = (gettid() << 16) + getpid();
            srcAddr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;  // events to subscribe to

            socketfd = socket( AF_NETLINK, (SOCK_DGRAM | SOCK_NONBLOCK), NETLINK_ROUTE );
            if ( socketfd < 0 )
            {
                loge( "Failed to create Netlink route socket. Event monitoring could not be started." );
                keepWorking = false;
                return;
            }

            ret = bind( socketfd, (sockaddr *) &srcAddr, sizeof( srcAddr ));
            if ( ret )
            {
                loge( "Failed to bind Netlink route socket. Event monitoring could not be started." );
                close( socketfd );
                keepWorking = false;
                return;
            }


            // 2. Configure epoll on the socket to allow reads with timeouts
            #define MAX_EPOLL_EVENTS 1
            epoll_event events[MAX_EPOLL_EVENTS];

            // Create epoll instance
            int epollfd = epoll_create( 1 );  // tip: "1" size argument is ignored, but must be > 0
            if ( epollfd < 0 )
            {
                loge( "Failed to create epoll instance. Event monitoring could not be started." );
                close( socketfd );
                keepWorking = false;
                return;
            }

            // Add socketfd to the watch list
            epoll_event event{0};
            event.events = EPOLLIN;         // Can append "|EPOLLOUT" for write events as well
            event.data.fd = socketfd;

            ret = epoll_ctl( epollfd, EPOLL_CTL_ADD, socketfd, &event );
            if ( ret < 0 )
            {
                loge( "Failed to add socket to epoll instance. Event monitoring could not be started." );
                close( epollfd );
                close( socketfd );
                keepWorking = false;
                return;
            }

            // 3. Loop to wait for incoming data
            logi( "Starting thread using network namespace '%s' ...", netns.c_str() );

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
                    loge( "epoll_wait() error on Netlink route socket (errno=%i %s); attempting to restart thread ...",
                          errno, strerror( errno ));
                    break;
                }
                else if ( nReady == 0 || !(events[0].events & EPOLLIN))
                {
                    continue;
                }

                // Read socket data
                long nCount = recv( socketfd, buf, sizeof( buf ), 0 );
                if ( nCount < 0 && errno != EAGAIN )
                {
                    logw( "recv() error on Netlink route socket (errno=%i %s)",
                          errno, strerror( errno ));
                    this_thread::sleep_for( 1s );
                }
                else if ( nCount > 0 )
                {
                    logY( "------------------------ Netlink Route Event len=%ld ----------------------", nCount );

                    // Process multipart message
                    // see https://www.man7.org/linux/man-pages/man7/netlink.7.html
                    nlmsghdr *msgHdr;

                    for (msgHdr = (nlmsghdr *) buf; NLMSG_OK(msgHdr, nCount); msgHdr = NLMSG_NEXT(msgHdr, nCount))
                    {
                        // end of multipart message
                        if ( msgHdr->nlmsg_type == NLMSG_DONE )
                        {
                            break;  // done
                        }
                        else if ( msgHdr->nlmsg_type == NLMSG_ERROR || msgHdr->nlmsg_type == NLMSG_OVERRUN )
                        {
                            loge( "Error message header found, discarding event ..." );
                            this_thread::sleep_for( 1s );
                            break;
                        }
                        else if ( msgHdr->nlmsg_type == NLMSG_NOOP )
                        {
                            continue; // No operation
                        }

                        // If an event is detected, invoke user callback
                        RouteEvent e = toRouteEvent( msgHdr );
                        if ( e.type == ROUTE_EVENT_NONE )
                            continue;

                        logM("Event ==> %s", e.toString().c_str() );

                        // Call user defined callback
                        try
                        {
                            routeCallback( e );
                        }
                        catch ( const std::exception &e )
                        {
                            loge( "User's routeCallback() finished with errors: %s", e.what());
                        }

                    }
                }


            }   //loop


            // Close socket
            close( epollfd );
            close( socketfd );

            logi( "Exiting thread using network namespace '%s' ...", netns.c_str() );

        }  /*outer while*/

    }



} // ns

