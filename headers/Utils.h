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
#ifndef UTILS_H
#define UTILS_H


#include <string>
#include <strings.h>

namespace lwsdk::Utils
{
    // Comparator for building case-insensitive maps, for example
    //     std::map<std::string, std::string, lwsdk::Utils::CaseInsensitiveComparator> myCiMap{};
    struct CaseInsensitiveComparator
    {
        bool operator()(const std::string& a, const std::string& b) const noexcept
        {
            return ::strcasecmp( a.c_str(), b.c_str()) < 0;
        }
    };

    /**
     * Returns the computer's local time zone string formatted as: 'MDT'
     */
    std::string localTimeZone();


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as: 'Mon Sep 04 00:25:05 2023 MDT'
     */
    std::string toDateTimeFull( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as: 'Mon Sep 04 12:25:05 AM 2023 MDT'
     */
    std::string toDateTimeFull12( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as: '2023-09-04 00:25:05 MDT'
     */
    std::string toDateTimeZ( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as:  '2023-09-04 12:25:05 AM MDT'
     */
    std::string toDateTimeZ12( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as: '2023-09-04 00:25:05'
     */
    std::string toDateTime( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as:  '2023-09-04 12:25:05 AM'
     */
    std::string toDateTime12( long epochMillis, bool useUTC = true );


    /**
     * Returns a date sting for the given epoch milliseconds
     * formatted as: '2023-09-04'
     */
    std::string toDate( long epochMillis, bool useUTC = true );


    /**
     * Returns a date sting for the given epoch milliseconds
     * formatted as: '09/04/2023'
     */
    std::string toDateUS( long epochMillis, bool useUTC = true );


    /**
     * Returns a time sting for the given epoch milliseconds
     * formatted as: '00:25:05'
     */
    std::string toTime( long epochMillis, bool useUTC = true );


    /**
     * Returns a date time sting for the given epoch milliseconds
     * formatted as: '12:25:05 AM'
     */
    std::string toTime12( long epochMillis, bool useUTC = true );


    /**
     * Get the current system clock time in microseconds.
     * @return he current system clock time in microseconds.
     */
    long currentTimeUsec();


    /**
     * Get the current UNIX epoch time in milliseconds.
     * @return the epoch time in milliseconds.
     */
    long currentTimeMillis();


    /**
     * Get the current UNIX epoch time in seconds.
     * @return the epoch time in seconds.
     */
    long currentTimeSeconds();

    /**
     * Executes a command in the shell as '/bin/sh -c' and return the command output
     * @param cmd  A shell command.
     * @return A string containing the command output.
     */
    std::string shellExec( const std::string& cmd );

    /**
     * Dumps the data at the given memory location to the console.
     * @param  address   Memory address to dump
     * @param  size      Number of bytes to dump
     */
    void memdump( const void *address, uint32_t size );

    /**
     * Return the username for the given system user ID.
     * @param userId  A system user ID
     * @return The username if found, "" if there is an error.
     */
    std::string getUserForId( long userId );

    /**
     * Return the group name for the given system group ID.
     * @param userId  A system group ID
     * @return The group name if found, "" if there is an error.
     */
    std::string getGroupForId( long groupId );
}

#endif /*UTILS_H*/
