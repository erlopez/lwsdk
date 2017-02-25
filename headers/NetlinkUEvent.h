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
#ifndef NETLINK_UEVENT_H
#define NETLINK_UEVENT_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "Exceptions.h"

namespace lwsdk
{
    // Size of the buffer to receive uevent data from kernel
    #define NETLINK_UEVENT_BUF_SZ 4096

    /**
     *  Hold UEvent data send from kernel.
     */
    struct UEvent
    {
        const std::string data;  // name=value multiline event's data

        explicit UEvent( const std::string &data ) : data( data ) {}

        /**
         * Returns value of an event property or blank "" if the event's data does not
         * contains the given property name.
         * @param propName   Name of the property to retrieve from the event's data
         */
        std::string valueOf( const std::string &propName ) const;


        /**
         * Returns integer value of an event property or defValue if the uevent data does not
         * contains the given property name.
         *
         * @param propName   Name of the property to retrieve from the event's data
         * @param defValue   Value to be returned if the uevent data does not
         *                   contains the given property name, or its value cannot be converted
         *                   to integer.
         */
        long intValueOf( const std::string &propName, long defValue = -1 ) const;
    };


    /**
     * User callback to receive kernel hot-plug events.
     * @param uevent  Event data
     */
    typedef std::function<void ( const UEvent &uevent )> UEventCallback_t;


    /**
     * Used to subscribe to kernel hot-plug events. Typical use include
     * detecting when USB devices are plugged/unplugged.
     */
    class NetlinkUEvent
    {
        UEventCallback_t  uEventCallback{nullptr};
        std::atomic_bool  keepWorking{false};
        std::thread       *uEventReaderThread{nullptr};
        char              buf[ NETLINK_UEVENT_BUF_SZ ]{0};

        void readerThread();

    public:
        /**
         * Establishes a connection to the kernel's hot-plug event stream
         * and forwards the event data to the given user-defined callback.
         *
         * @param uEventCallback User-defined function to receive event notifications
         */
        explicit NetlinkUEvent( const UEventCallback_t& uEventCallback );

        virtual ~NetlinkUEvent();

        /**
         * Returns true if the service is running, false otherwise.
         */
        bool isRunning() { return keepWorking; }

    };

}
#endif //NETLINK_UEVENT_H
