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
#ifndef STRINGS_H
#define STRINGS_H


#include <vector>
#include <regex>
#include <string>

namespace lwsdk::Strings
{
    /**
     * Lowercases s string
     */
    std::string toLowerCase( const std::string &s );

    /**
     * Uppercases a string
     */
    std::string toUpperCase( const std::string &s );

    /**
     * Return the given multi-line string indented by N number of spaces
     */
    std::string indent( const std::string& s, int n );


    /**
     * Return the given string repeated back to back N number of times.
     */
    std::string repeat( const std::string& s, int n );


    /**
     * Reads a whole text file and returns it as separated string lines.
     * Throws IOException if the operation fails.
     * Lines larger than 8192 characters will be truncated.
     */
    std::vector<std::string> getFileAsLines( const std::string& pathname );


    /**
     * Reads a whole text file and returns it as a multi-line string.
     * Throws IOException if the operation fails.
     */
    std::string getFileAsString( const std::string& pathname );


    /**
     * Saves the given string s to a file pointed by pathname.
     * Throws IOException if the operation fails.
     */
    void saveStringAsFile( const std::string& s, const std::string& pathname );

    /**
     * Returns true is the given string is either "true", "enable[d]", "y", "yes",
     * or "1" (case insensitive), otherwise returns false.
     * Any leading/trailing spaces are ignored.
     */
    bool parseBool( const std::string& s );

    /**
     * Converts the given string to int, returns defVal if the conversion fails.
     * Attempts to convert values up to the first non-valid character found.
     */
    int parseInt( const std::string& s, const int& defVal );

    /**
     * Converts the given string to long, returns defVal if the conversion fails.
     * Attempts to convert values up to the first non-valid character found.
     */
    long parseLong( const std::string& s, const long& defVal );

    /**
     * Converts the given hex string to long, returns defVal if the conversion fails.
     * Attempts to convert values up to the first non-valid character found.
     * If present, the 0x prefix is ignored.
     */
    long parseHex( const std::string& s, const long& defVal );

    /**
     * Converts the given string to double, returns defVal if the conversion fails.
     * Attempts to convert values up to the first non-valid character found.
     */
    double parseDouble( const std::string& s, const double& defVal );

    /**
     * Returns true if the given string starts with the given prefix
     */
    bool startsWith( const std::string& s, const std::string& prefix );

    /**
     * Returns true if the given string ends with the given suffix
     */
    bool endsWith( const std::string& s, const std::string& suffix );

    /**
     * Do a find and replace in the given string and returns the result
     * The replacement string accepts $1, $2, etc.. for captured groups in find string
     */
    std::string replaceAll( const std::string& s, const std::string& sFindRegex, const std::string& sReplace, bool isCaseSensitive = true );

    /**
     * Returns the first substring matching the provided regular expression; or a blank
     * string if no match is found.
     */
    std::string findMatch( const std::string& s, const std::string& sRegex, bool isCaseSensitive = true );

    /**
     * Returns true if the given string matches the provided regular expression
     */
    bool matches( const std::string& s, const std::string& sRegex, bool isCaseSensitive = true );

    /**
     * Returns true if the given string contains a substring matching the provided
     * regular expression.
     */
    bool contains( const std::string& s, const std::string& sRegex, bool isCaseSensitive = true );

    /**
     * Splits a string by the given regex delimiter expression.
     */
    std::vector<std::string> split( const std::string& s, const std::regex& delimRegex );

    /**
     * Splits a string by the given regex delimiter expression.
     */
    std::vector<std::string> split( const std::string& s, const std::string& delimRegex );

    /*
     * Return substrings matching the given regex expression.
     */
    std::vector<std::string> findMatches( const std::string& s, const std::string& matchRegex );

    /**
     * Trim blank spaces at the begin of the string
     */
    std::string ltrim( const std::string &s );

    /**
     * Trim blank spaces at the end of the string
     */
    std::string rtrim( const std::string &s );

    /**
     * Trim blank spaces at each end of the string
     */
    std::string trim( const std::string &s );

    /**
     * Return true if the given string is empty or only
     * contains blank spaces [' ' \t \n \r \f \v ].
     */
    bool isEmpty( const std::string &s );


}


#endif //STRINGS_H
