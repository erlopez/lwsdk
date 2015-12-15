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
#include "Utils.h"

#include <cstdint>
#include <cctype>
#include <cstdio>
#include <time.h>    // for localtime_r() thread safe
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <string>


using namespace std;
using namespace std::chrono;


namespace lwsdk::Utils
{
    /**
     * Returns a date time sting for the given format
     * @param epochMillis   Milliseconds sind UNIX epoch
     * @param fmt           Date time format string, see https://cplusplus.com/reference/iomanip/put_time/?kw=put_time
     * @param useUTC        True to use GMT/UTC timezone, otherwise use local time
     * @return              Formatted string
     */
    static std::string toDateTimeString( long epochMillis, const char *fmt, bool useUTC )
    {
        time_t t = epochMillis / 1000;
        std::stringstream ss;
        struct tm ts{};

        if ( useUTC )
            ss << put_time( gmtime_r( &t, &ts ), fmt );
        else
            ss << put_time( localtime_r( &t, &ts ), fmt );

        return ss.str();
    }


    std::string localTimeZone()
    {
        return toDateTimeString( currentTimeMillis(), "%Z", false );
    }


    std::string toDateTimeFull( long epochMillis, bool useUTC  )
    {
        return toDateTimeString( epochMillis, "%a %b %d %T %Y", useUTC );
    }


    std::string toDateTimeFull12( long epochMillis, bool useUTC  )
    {
        return toDateTimeString( epochMillis, "%a %b %d %r %Y", useUTC );
    }


    std::string toDateTimeZ( long epochMillis, bool useUTC  )
    {
        return toDateTimeString( epochMillis, "%Y-%m-%d %H:%M:%S %Z", useUTC );
    }


    std::string toDateTimeZ12( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%Y-%m-%d %I:%M:%S %p %Z", useUTC );
    }


    std::string toDateTime( long epochMillis, bool useUTC  )
    {
        return toDateTimeString( epochMillis, "%Y-%m-%d %H:%M:%S", useUTC );
    }


    std::string toDateTime12( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%Y-%m-%d %I:%M:%S %p", useUTC );
    }


    std::string toDate( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%Y-%m-%d", useUTC );
    }


    std::string toDateUS( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%m/%d/%Y", useUTC );
    }


    std::string toTime( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%H:%M:%S", useUTC );
    }


    std::string toTime12( long epochMillis, bool useUTC )
    {
        return toDateTimeString( epochMillis, "%I:%M:%S %p", useUTC );
    }



    long currentTimeUsec()
    {
        long t = system_clock::now().time_since_epoch().count();
        return t * 1000000 * system_clock::period::num / system_clock::period::den;  // convert to ms
    }


    long currentTimeMillis()
    {

        long t = system_clock::now().time_since_epoch().count();
        return t * 1000 * system_clock::period::num / system_clock::period::den;  // convert to ms
    }


    long currentTimeSeconds()
    {
        return currentTimeMillis() / 1000;
    }


    std::string shellExec( const std::string &cmd )
    {
        char buffer[128];
        std::string result;

        FILE *fp = popen( cmd.c_str(), "r" );

        if ( fp == NULL )
        {
            result = result + "!IO_ERROR: " + cmd;
            return result;
        }

        while ( !feof( fp ))
        {
            if ( fgets( buffer, sizeof( buffer ), fp ) != NULL )
                result += buffer;
        }

        //int st=pclose(fp); if(WIFEXITED(st)) printf("Exit code: %i\n", WEXITSTATUS(st) );
        pclose( fp );

        return result;
    }


    void memdump( const void *address, uint32_t size )
    {
        uint8_t *mem = (uint8_t *) ((uint64_t) address & 0xFFFFFFFFFFFFFFF0ull); // start dump at base 16-byte block
        uint32_t i, line, linecount = (size + ((uint64_t) address - (uint64_t) mem)) / 16 + 1;


        printf( "\n  MEM DUMP AT %p:\n  ----------------------------------------------------------------------------------------\n", address );

        for ( line = 0; line < linecount; line++ )
        {
            printf( "  [%04x] %08lx: ", (uint32_t) (line * 16), ((uint64_t) mem));

            // hex data
            for ( i = 0; i < 16; i++ )
            {
                if ((&mem[i] >= (uint8_t *) address) && (&mem[i] < (uint8_t *) address + size))
                    printf( "%02X ", mem[i] );
                else
                    printf( ".. " );

                if ( i == 7 )
                    printf( "- " );
            }

            printf( " " );

            // ascii data
            for ( i = 0; i < 16; i++ )
            {
                if ((&mem[i] >= (uint8_t *) address) && (&mem[i] < (uint8_t *) address + size))
                    printf( "%c", isprint( mem[i] ) ? mem[i] : '.' );
                else
                    printf( "." );
            }

            mem += 16;
            printf( "\n" );
        }

        printf( "  ----------------------------------------------------------------------------------------\n\n" );
    }

} //ns