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
#ifndef FILES_H
#define FILES_H

#include "Strings.h"
#include <filesystem>

namespace lwsdk::Files
{
    // Object returned by getFileInfo()
    struct FileInfo {
        bool exists {false};     // true if pathname exists
        bool isFile {false};     // true if pathname is a regular file
        bool isDir {false};      // true if pathname is a directory
        int  mode {0};           // like chmod, decode using macros: S_IRUSR, S_IWUSR, S_IXUSR;  S_IRGRP, S_IWGRP, S_IXGRP;  S_IROTH, S_IWOTH, S_IXOTH,
        long uid  {-1};          // id for the file's user
        long gid  {-1};          // id for the file's group
        long size {-1};          // file size
        long lastModified {-1};  // epoch seconds
        int  errorNo{0};         // errno for the IO operation, 0=OK
        std::string username;    // obtained on best effort, blank if it cannot be obtained
        std::string group;       // obtained on best effort, blank if it cannot be obtained

        inline std::string toString() const
        {
            char buf[256];
            snprintf( buf, sizeof(buf), "FileStatus{ exists:%s, isFile:%s, isDir:%s, uid:%ld, gid:%ld, "
                                        "mode:'%04o', size:%ld, lastModified:%ld, username:'%s', group:'%s', errorNo:%d }",
                                        exists ? "true" : "false", isFile ? "true" : "false", isDir ? "true" : "false",
                                        uid, gid, mode, size, lastModified, username.c_str(), group.c_str(), errorNo );
            return buf;
        }
    };

    // mkpath() - Compiler rule to check all vardic template arguments are string type
    // see https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/
    template<typename... Ts>  using ALLSTRINGS = typename
       std::enable_if<std::conjunction<std::is_convertible<Ts, std::string>...>::value>::type;

    /**
     * Makes a path from multiple path elements.
     * @param pathname   Base directory starting the path
     * @param subpaths   One or more string elements to be concatenated
     * @return Final concatenate path of all given string elements separated by '/'
     */
    template<typename... Ts, typename = ALLSTRINGS<Ts...> >
    std::string mkpath( const std::string &pathname, const Ts &... subpaths )
    {
        std::filesystem::path p( Strings::replaceAll(pathname, "/+$", "") );

        // fold expression to concat elements
        p = ( p / ... / Strings::replaceAll(subpaths, "^/+|/+$", "") );

        return p.string();
    }


    /**
     * Copies file or directory recursively. If destination exists, it is
     * overwritten (if a file), merged (if a directory).
     *
     * Remarks on copying a directory into another directory:
     *   - copy("dir1", "dir2" );
     *        if dir2 does not exists, dir2 is created with contents of dir1
     *        if dir2 exists, contents of dir1 are merged into dir2
     *   - copy("dir1", "dir2/dir1" );
     *        dir2 must exists
     *        a dir1 copy is created (or merged if already exists) inside dir2
     *
     * See https://en.cppreference.com/w/cpp/filesystem/copy for details.
     * @param fromPathname Original file or directory to be copies
     * @param toPathname   Destination file or folder, overwriting existing
     * @throw IOException if the operation fails.
     */
    void copy( const std::string& fromPathname, const std::string& toPathname );

    /**
     * Renames or move a file or directory as in mv.
     * @param fromPathname Original file or directory to be renamed/moved
     * @param toPathname   New name or location
     * @throw IOException if the operation fails.
     */
    void move( const std::string& fromPathname, const std::string& toPathname );

    /**
     * Alias for move. Renames or move a file or directory as in mv.
     * @param fromPathname Original file or directory to be renamed/moved
     * @param toPathname   New name or location; it cannot exist already
     * @throw IOException if the operation fails.
     */
    inline void rename( const std::string& fromPathname, const std::string& toPathname )
    {
        move( fromPathname, toPathname );
    }

    /**
     * Removes a file or directory to be removed recursively as in rm -r.
     * Typically there is no error if the target does not exists, or
     * you have no permissions to remove; in these cases the function return 0.
     * @param pathname File or directory to be removed recursively
     * @return Number of files or directories removed.
     * @throw IOException if the operation fails.
     */
    uintmax_t remove( const std::string& pathname );

    /**
     * Returns the path of the current directory
     */
    std::string getCurrentDir(); 

    /**
     * Change the program's current directory as in chdir.
     * @param pathname     Path to new directory
     * @throw IOException if the operation fails.
     */
    void changeDir( const std::string& pathname );

    /**
     * Creates one or more parent/child directories as in makeDir -p.
     * @param pathname Directory structure to be created
     * @throw IOException if the operation fails.
     */
    void makeDir( const std::string& pathname );


    /**
     * Test if the file pointed by pathname exists.
     * @param pathname Path to the file or directory to be tested.
     * @return True if the file or directory exist, false otherwise.
     */
    bool exists( const std::string& pathname );

    /**
     * Test if the pathname points to a directory.
     * @param pathname Path to directory to be tested.
     * @return True if the path exist and it is a directory exist, false otherwise.
     */
    bool isDir( const std::string& pathname );

    /**
     * Test if the pathname points to a File.
     * @param pathname Path to file to be tested.
     * @return True if the path exist and it is a file exist, false otherwise.
     */
    bool isFile( const std::string& pathname );

    /**
     * Test if the pathname points to a symlink.
     * @param pathname Path to file to be tested.
     * @return True if the path exist and it is a symlink exist, false otherwise.
     */
    bool isSymlink( const std::string& pathname );

    /**
     * Returns the file size of the given pathname.
     * @param pathname Path to file to be tested.
     * @return file size of the given pathname.
     * @throw IOException if the operation fails.
     */
    uintmax_t getFileSize( const std::string& pathname );

    /**
     * Returns the epoch milliseconds of the file's last update.
     * @param pathname Path to file to be tested.
     * @return epoch milliseconds of the file's last update.
     * @throw IOException if the operation fails.
     */
    long getLastUpdated( const std::string& pathname );


    /**
     * Returns the file time type converted to UNIX epoch milliseconds.
     * @param t  File time from a directory_entry
     * @return   Time converted to UNIX epoch milliseconds
     */
    long fileTimeToMillis( std::filesystem::file_time_type const &fileTime );


    /**
     * Copies data from IN stream into OUT stream until the end of the IN stream
     * is reached. Throws IOException if the operation fails.
     */
    void streamCopy( std::istream& in, std::ostream& out );

    /**
     * Retrieve information about a file or directory. Links
     * are followed to the target file or directory.
     * @param pathname  Path to file, directory, or link.
     * @return  FileInfo structure. The information is valid only
     *          if FileInfo.errorNo == 0;
     */
    FileInfo getFileInfo( const std::string& pathname );

    
} // ns


#endif /*FILES_H */
