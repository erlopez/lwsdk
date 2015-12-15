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
#include "Files.h"
#include "Exceptions.h"

#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;
using namespace std;

namespace lwsdk::Files
{
    
    void copy( const std::string& fromPathname, const std::string& toPathname )
    {
        error_code ec;

        fs::copy( fromPathname, toPathname, fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec );

        if ( ec.value() != 0 )
            throw IOException("copy() - Failed to copy file or directory from " +
                              fromPathname + " to " + toPathname + " - " + ec.message() );
    }


    void move( const std::string& fromPathname, const std::string& toPathname )
    {
        error_code ec;

        fs::rename( fromPathname, toPathname, ec );

        if ( ec.value() != 0 )
            throw IOException("move() - Failed to move/rename file or directory from " +
                              fromPathname + " to " + toPathname + " - " + ec.message() );
    }


    uintmax_t remove( const std::string& pathname )
    {
        error_code ec;

        uintmax_t n = fs::remove_all( pathname, ec );

        if ( ec.value() != 0 )
            throw IOException("remove() - Failed to remove file or directory " + pathname + " - " + ec.message() );

        return n;
    }


    string getCurrentDir()
    {
        return filesystem::current_path();
    }


    void changeDir( const std::string& pathname )
    {
        error_code ec;

        fs::current_path( pathname, ec );

        if ( ec.value() != 0 )
            throw IOException("chdir() - Failed to change to directory " + pathname + " - " + ec.message() );
    }


    void makeDir( const std::string& pathname )
    {
        error_code ec;

        fs::create_directories( pathname, ec );

        if ( ec.value() != 0 )
            throw IOException("makeDir() - Failed to create directory " + pathname + " - " + ec.message() );
    }


    bool exists( const std::string& pathname )
    {
        return fs::exists( pathname );
    }


    bool isDir( const std::string& pathname )
    {
        return fs::is_directory( pathname );
    }


    bool isFile( const std::string& pathname )
    {
        return fs::is_regular_file( pathname );
    }


    bool isSymlink( const std::string& pathname )
    {
        return fs::is_symlink( pathname );
    }


    uintmax_t getFileSize( const std::string& pathname )
    {
        error_code ec;

        uintmax_t sz = fs::file_size( pathname, ec );

        if ( ec.value() != 0 )
            throw IOException( "getFileSize() - Failed to get file size " + pathname + " - " + ec.message());

        return sz;
    }

    long getLastUpdated( const std::string& pathname )
    {
        error_code ec;

        auto fileTs = fs::last_write_time( pathname, ec );

        if ( ec.value() != 0 )
            throw IOException("getLastUpdated() - Failed to get file time " + pathname + " - " + ec.message() );

        return Files::fileTimeToMillis( fileTs );

    }




    long fileTimeToMillis( std::filesystem::file_time_type const &fileTime )
    {
        // find drift between file_clock and system clock
        long dt = fs::file_time_type::clock::now().time_since_epoch().count() -
                  chrono::system_clock::now().time_since_epoch().count();

        // convert file time to sys clock
        long sec = fileTime.time_since_epoch().count() - dt;

        // convert to millis
        return sec * 1000 * chrono::system_clock::period::num / chrono::system_clock::period::den;
    }


    void streamCopy( std::istream& in, std::ostream& out )
    {
        try
        {
            const int BUF_SZ = 1024;
            auto buf = make_unique<char[]>(BUF_SZ);

            while ( !in.eof() )
            {
                in.read( buf.get(), BUF_SZ );
                out.write( buf.get(), in.gcount() );
            }

        }
        catch ( const exception &e )
        {
            throw IOException( string("streamCopy() - IOException, ") + e.what() );
        }

    }


} //ns
