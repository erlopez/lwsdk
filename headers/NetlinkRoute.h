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
#ifndef NETLINK_ROUTE_H
#define NETLINK_ROUTE_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "Exceptions.h"
#include <linux/rtnetlink.h>

namespace lwsdk
{
    // Size of the buffer to receive route data from kernel
    #define NETLINK_ROUTE_BUF_SZ 8192


    // Route event types
    enum RouteEventType {
        ROUTE_EVENT_NONE,     // No event
        ROUTE_EVENT_NEWROUTE, // Routing table changed, due to IP address changes
        ROUTE_EVENT_DELROUTE, // Routing table changed, due to IP address changes
        ROUTE_EVENT_NEWADDR,  // IP address added
        ROUTE_EVENT_DELADDR,  // IP address removed
        ROUTE_EVENT_NEWLINK,  // Cable connected or disconnected (carrier detect), USB-to-Ethernet adapter plugged
        ROUTE_EVENT_DELLINK   // USB-to-Ethernet adapter unplugged
    };

    // Hold route event send from kernel
    struct RouteEvent
    {
        RouteEventType type;      // event type enumeration constant
        std::string ifname;       // valid with event *ADDR and *LINK
        std::string ipaddr;       // valid with event NEWADDR
        bool isUp;                // valid with event NEWLINK
        bool isRunning;           // valid with event NEWLINK, carrier detect

        std::string toString() const;
    };


    /**
     * User callback to receive kernel hot-plug events.
     * @param routeEvent  Event data
     */
    typedef std::function<void ( const RouteEvent &routeEvent )> RouteEventCallback_t;


    /**
     * Used to subscribe to kernel hot-plug events. Typical use include
     * detecting when ethernet cables or USB-to-ethernet adapters are plugged/unplugged.
     */
    class NetlinkRoute
    {
        RouteEventCallback_t  routeCallback{nullptr};
        std::atomic_bool      keepWorking{false};
        std::thread           *routeReaderThread{nullptr};
        nlmsghdr              buf[NETLINK_ROUTE_BUF_SZ/sizeof(nlmsghdr)]{0};
        std::string           netns;

        void readerThread();

    public:
        /**
         * Establishes a connection to the kernel's rtnetlink stream and forwards
         * add/remove RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE events
         * to the given user-defined callback. These events are used to monitor
         * things like an ethernet cable being plugged/unplugged, and an interface IP
         * address or the system's routing table being changed. See NETLINK_ROUTE at
         * https://man7.org/linux/man-pages/man7/rtnetlink.7.html
         *
         * By default, changes in the application's inherited network namespace are monitored.
         * To monitor changes in another network namespace set netns to the name of the
         * network namespace to be monitored. WARNING setting netns requires the program to
         * run with root privileges (sudo), or that the executable file is owned by root and has
         * the setuid bit set, for example:
         *     sudo chown root /path/to/executable
         *     sudo chmod u+s /path/to/executable
         *
         * The list of namespaces can be seen with either:
         *     ip netns list
         *     ls /var/run/netns
         *
         * @param routeCallback User-defined function to receive event notifications
         * @param netns         Optional network namespace. Set to blank "" to use the default network
         *                      namespace. Setting this to other than blank "" requires root privileges.
         */
        explicit NetlinkRoute( const RouteEventCallback_t& routeCallback, const std::string& netns = "" );

        virtual ~NetlinkRoute();

        /**
         * Returns true if the service is running, false otherwise.
         */
        bool isRunning() { return keepWorking; }

    };

}
#endif //NETLINK_ROUTE_H
