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
#include <algorithm>
#include <fstream>
#include "Strings.h"
#include "Files.h"
#include "Exceptions.h"

using namespace std;

namespace lwsdk::Strings
{
    static const string EMPTY_STRING;


    string toLowerCase( const string &s )
    {
        string s2 = s;
        transform( s2.begin(), s2.end(), s2.begin(),
                        []( const char v ) { return tolower( v ); } );
        return s2;
    }


    string toUpperCase( const string &s )
    {
        string s2 = s;
        transform( s2.begin(), s2.end(), s2.begin(),
                        []( const char v ) { return toupper( v ); } );
        return s2;
    }


    string indent( const string& s, int n )
    {
        if ( n > 0 )
        {
            string spaces( n, ' ' );
            return spaces + regex_replace( s, regex( "\\n" ), "\n" + spaces );
        }
        else
        {
            return s;
        }

    }


    string repeat( const string& s, int n )
    {
        ostringstream oss;

        while ( n-- > 0 )
            oss << s;

        return oss.str();
    }

    
    vector<string> getFileAsLines( const string& pathname )
    {
        try
        {
            vector<string> lines;
            const int BUF_SZ = 8192;
            auto buf = make_unique<char[]>(BUF_SZ);

            fstream fs( pathname, ios_base::in );
            if ( fs.fail() )
                throw IOException( string("Failed to open file ") + pathname );

            while ( !fs.eof() )
            {
                fs.getline( buf.get(), BUF_SZ );

                if ( fs.bad() )
                {
                    fs.close();
                    throw IOException( string("Error while reading file ") + pathname);
                }

                lines.emplace_back( buf.get(), 0, fs.gcount() );
            }

            fs.close();

            return lines;
        }
        catch ( const exception &e )
        {
            throw IOException( string("getFileAsLines() - IOException, ") + e.what() );
        }
    }

    
    string getFileAsString( const string& pathname )
    {
        ostringstream oss;

        try
        {
            //oss << std::ifstream( pathname ).rdbuf(); // <-- not good, no error if file does not exists

            fstream fs( pathname, ios_base::in );
            if ( fs.fail() )
                throw IOException( string("Failed to open file ") + pathname );

            try
            {
                Files::streamCopy( fs, oss );
                fs.close();
            }
            catch ( const exception &e )
            {
                fs.close();
                throw e;
            }

        }
        catch ( const exception &e )
        {
            throw IOException( string("getFileAsString() - IOException, ") + e.what() );
        }

        return oss.str();
    }


    void saveStringAsFile( const string& s, const string& pathname )
    {
        istringstream iss(s);

        try
        {
            fstream fs( pathname, ios_base::out | ios_base::binary );
            if ( fs.fail() )
                throw IOException( string("Failed to create file ") + pathname );

            try
            {
                Files::streamCopy( iss, fs );
                fs.close();
            }
            catch ( const exception &e )
            {
                fs.close();
                throw e;
            }

        }
        catch ( const exception &e )
        {
            throw IOException( string("saveStringAsFile() - IOException, ") + e.what() );
        }


    }


    bool parseBool( const string& s )
    {
        static const regex IS_BOOL( "^(true|enabled?|1|y|yes)$", regex::ECMAScript | regex::icase );
        return regex_match( trim(s), IS_BOOL );
    }


    int parseInt( const string& s, const int& defVal )
    {
        try { return stoi( s ); } catch (...) { return defVal; }
    }


    long parseLong( const string& s, const long& defVal )
    {
        try { return stol( s ); } catch (...) { return defVal; }
    }


    double parseDouble( const string& s, const double& defVal )
    {
        try { return stod( s ); } catch (...) { return defVal; }
    }


    long parseHex( const string& s, const long& defVal )
    {
        try { return stol( s, 0, 16 ); } catch (...) { return defVal; }
    }


    bool startsWith( const string& s, const string& prefix )
    {
        if ( prefix.length() > s.length() )
            return false;

        size_t n = 0;
        while ( n < prefix.length() && s[n] == prefix[n] )
            n++;

        return n == prefix.length();
    }


    bool endsWith( const string& s, const string& suffix )
    {
        if ( suffix.length() > s.length() )
            return false;

        size_t n = s.length() - suffix.length();
        size_t i = 0;

        while ( n < s.length() && s[n] == suffix[i++] )
            n++;

        return n == s.length();
    }


    string replaceAll( const string& s, const string& sFindRegex, const string& sReplace, bool isCaseSensitive )
    {
        regex r(sFindRegex, regex_constants::ECMAScript |
                                (isCaseSensitive ? static_cast<std::regex_constants::syntax_option_type>(0)
                                                 : regex_constants::icase) );
        return regex_replace( s, r, sReplace);
    }


    string findMatch( const string& s, const string& sRegex, bool isCaseSensitive )
    {
        regex r(sRegex, regex_constants::ECMAScript |
                        (isCaseSensitive ? static_cast<std::regex_constants::syntax_option_type>(0)
                                         : regex_constants::icase) );
        smatch m;
        regex_search( s, m, r );
        return m.empty() ? EMPTY_STRING : m[0];
    }


    bool matches( const string& s, const string& sRegex, bool isCaseSensitive )
    {
        regex r(sRegex, regex_constants::ECMAScript |
                        (isCaseSensitive ? static_cast<std::regex_constants::syntax_option_type>(0)
                                         : regex_constants::icase) );
        return regex_match( s, r );
    }


    bool contains( const string& s, const string& sRegex, bool isCaseSensitive )
    {
        regex r(sRegex, regex_constants::ECMAScript |
                        (isCaseSensitive ? static_cast<std::regex_constants::syntax_option_type>(0)
                                         : regex_constants::icase) );
        return regex_search( s, r );
    }
    
    vector<string> split( const string& s, const string& delimRegex )
    {
        regex r(delimRegex);
        return split( s, r );
    }


    vector<string> split( const string& s, const regex& delimRegex )
    {
        vector<string> v;
        sregex_iterator itBegin = std::sregex_iterator( s.begin(), s.end(), delimRegex );
        sregex_iterator itEnd = sregex_iterator();
        smatch m;

        if ( itBegin == itEnd )
        {
            v.push_back( s );
        }
        else
        {
            for ( auto it = itBegin; it != itEnd; ++it )
            {
                m = *it; // copy into outer scope var
                v.push_back( m.prefix());
            }

            // last chunk
            if ( !m.empty())
                v.push_back( m.suffix());
        }
        
        return v;
    }

    vector<string> findMatches( const string& s, const string& matchRegex )
    {
        vector<string> v;
        regex r( matchRegex );
        sregex_token_iterator iter( s.begin(), s.end(), r );
        sregex_token_iterator end;
        v.clear();

        while ( iter != end )
        {
            v.push_back( iter->str() );
            iter++;
        }

        return v;
    }


    string ltrim( const string &s )
    {
        auto testFunct = []( unsigned char ch ) { return !isspace( ch ); };
        auto firstNonSpacePos = find_if( s.begin(), s.end(), testFunct );

        return { string( firstNonSpacePos, s.end() ) };
    }


    string rtrim( const string &s )
    {
        auto testFunct = []( unsigned char ch ) { return !isspace( ch ); };
        auto lastNonSpacePos = find_if( s.rbegin(), s.rend(), testFunct ).base();

        return { string( s.begin(), lastNonSpacePos ) };
    }


    string trim( const string &s )
    {
        auto testFunct = []( unsigned char ch ) { return !isspace( ch ); };
        auto firstNonSpacePos = find_if( s.begin(), s.end(), testFunct  );
        auto lastNonSpacePos = find_if( s.rbegin(), s.rend(), testFunct ).base();

        long diff = distance(firstNonSpacePos, lastNonSpacePos);

        return { diff < 0 ? EMPTY_STRING : string( firstNonSpacePos, lastNonSpacePos ) };
    }


    bool isEmpty( const string &s )
    {
        return trim(s).empty();
    }

}
