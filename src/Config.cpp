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
#include <cstdlib>
#include <map>
#include <sstream>
#include <regex>
#include <utility>
#include <iomanip>
#include <filesystem>

#include "Config.h"
#include "Strings.h"
#include "Exceptions.h"


#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"

using namespace std;

namespace lwsdk::Config
{
    // Store configuration options definitions
    struct ConfigOption
    {
        const string shortName;
        const string longName;
        const ConfigOptionType type;
        const bool isRequired;
        const string defVal;
        const string docstr;

        ConfigOption( string shortName, string longName, const ConfigOptionType type,
                      const bool isRequired, string defVal, string docstr )
                : shortName( std::move( shortName )), longName( std::move( longName )), type( type ),
                  isRequired( isRequired ),  defVal( std::move( defVal )), docstr( std::move( docstr ))
        {}

        string optionNames() const
        {
            return !shortName.empty()
                   ? string( "-" ) + shortName + ", --" + longName
                   : string( "--" ) + longName;
        }
    };

    // Globals
    static const char* MISSING_VAL_TAG = "!?!?!?-MIZZING-VAL-TAG";
    static map<string, string> optionVars;
    static map<string, string> envVars;
    static vector<ConfigOption> configOptions;
    static string programName;
    static string programDir;
    static string programArgs;
    static int    programArgsCount = 0;


    void reset()
    {
        optionVars.clear();
        envVars.clear();
    }

    void resetDefinitions()
    {
        configOptions.clear();
    }

    void defineConfigOption( ConfigOptionType type, const string &longName, const std::string &defVal, const string &docstr )
    {
        defineConfigOption( type, '\0', longName, defVal, docstr );
    }

    
    void defineConfigOption( ConfigOptionType type, const char shortName, const string &longName, const std::string &defVal, const string &docstr )
    {
        string sname( shortName == '\0' ? 0 : 1, shortName );  // set to "" if shortName is \0
        configOptions.emplace_back( sname, longName, type, false, defVal, docstr );

        // push default value
        optionVars[ longName ] = defVal;
    }


    void defineConfigOption( ConfigOptionType type, const string &longName, const string &docstr )
    {
        defineConfigOption( type, '\0', longName, docstr );
    }


    void defineConfigOption( ConfigOptionType type, const char shortName, const string &longName, const string &docstr )
    {
        string sname( shortName == '\0' ? 0 : 1, shortName );  // set to "" if shortName is \0
        configOptions.emplace_back( sname, longName, type, true, "", docstr );

    }


    /**
     * Test if a config option for the given long name
     */
    static bool configOptionExists( const string& longName )
    {
        auto it = std::find_if(configOptions.begin(), configOptions.end(),
                                        [&longName](const ConfigOption& item){ return longName == item.longName; });

        return it != configOptions.end();
    }


    /**
     * Translate short to long configuration option name
     */
    static string longConfigOptionFor( const string& shortName )
    {
        auto it = std::find_if(configOptions.begin(), configOptions.end(),
                                        [&shortName](const ConfigOption& item){ return shortName == item.shortName; });

        return it == configOptions.end() ? "" : it->longName;
    }


    /**
     * Validate user-given options against configuration options expected by program
     */
    static string validateConfigOptions( bool useStrictCheck )
    {
        static const regex IS_BOOL( "^(false|true|enabled?|disabled?|1|0|y|n|yes|no)$" );
        static const regex IS_INT( "^[-+]?\\d+$" );
        static const regex IS_UINT( "^\\+?\\d+$" );
        static const regex IS_DECIMAL( "^[-+]?\\d+(.\\d+)?$" );

        for ( const ConfigOption& co : configOptions )
        {
            // Retrieve option name from args[], val is blank "" if not found
            string val = get( co.longName );
            if ( val.empty() && !co.shortName.empty() )
                val = get( co.shortName );

            // Special case for booleans
            //  - an empty value is assumed "false" (absence of the flag)
            //  - a value of MISSING_VAL_TAG is taken as "true" since booleans don't need explicit
            //    TRUE values their (presence of the flag)
            if ( co.type == BOOL )
            {
                if ( val.empty() )
                    val = "false";
                else if (val == MISSING_VAL_TAG)
                    val = "true";
            }

            // Check required option
            if ( co.isRequired && (val.empty() || val == MISSING_VAL_TAG) )
                return string( "Missing option: " ) + co.optionNames();

            // Validate data types
            if ( co.type == ConfigOptionType::BOOL )
            {
                val = Strings::toLowerCase( Strings::trim( val ));
                if ( !regex_match( val, IS_BOOL ))
                    return string( "Option " ) + co.optionNames() + " expects {false|true|1|0|y[es]|n[o]|enable[d]|disable[d]}; '" + val + "' is invalid.";

                // load into optionVars list
                optionVars[co.longName] = Strings::parseBool(val) ? "true" : "false";
            }
            else if ( co.type == ConfigOptionType::UINT )
            {
                if ( val == MISSING_VAL_TAG )
                    val.clear();
                else
                    val = Strings::trim( val );

                if ( !regex_match( val, IS_UINT ))
                    return string( "Option " ) + co.optionNames() + " expects a positive integer value; '" + val + "' is invalid.";

                // load into optionVars list
                optionVars[co.longName] = val;
            }
            else if ( co.type == ConfigOptionType::INT )
            {
                if ( val == MISSING_VAL_TAG )
                    val.clear();
                else
                    val = Strings::trim( val );

                if ( !regex_match( val, IS_INT ))
                    return string( "Option " ) + co.optionNames() + " expects an integer value; '" + val + "' is invalid.";

                // load into optionVars list
                optionVars[co.longName] = val;
            }
            else if ( co.type == ConfigOptionType::DECIMAL )
            {
                if ( val == MISSING_VAL_TAG )
                    val.clear();
                else
                    val = Strings::trim( val );

                if ( !regex_match( val, IS_DECIMAL ))
                    return string( "Option " ) + co.optionNames() + " expects a integer, float, or double value; '" + val + "' is invalid.";

                // load into optionVars list
                optionVars[co.longName] = val;
            }
            else
            {
                // load into optionVars list as text
                optionVars[co.longName] = val == MISSING_VAL_TAG ? "" : val;
            }

        } // for


        // Convert any left missing values to blank
        // Reject unknown options when using strict mode
        for( auto& pair : optionVars )
        {
            if ( useStrictCheck && !configOptionExists( pair.first ) && !Strings::matches( pair.first, "^\\$[\\d#*]$" ))    // ok to pass $# $1..99 $*
                return string( "Invalid option: " ) + pair.first;

            if ( pair.second == MISSING_VAL_TAG )
                pair.second.clear();
        }
        

        return "";
    }


    string loadConfigArgs( int argc, char **argv, bool useStrictCheck )
    {
        static const regex OPT_NAME_SHORT( "^-(\\w)$" );
        static const regex OPT_NAME_LONG( "^--([-\\w]+)$" );
        static const regex OPT_SHORT_WITH_VAL( "^-(\\w)=?(.*)$" );
        static const regex OPT_LONG_WITH_VAL( "^--([-\\w]+)=(.*)$" );

        // save program's directory and full name
        programArgs.clear();
        programArgsCount = 0;

        if ( argc > 0 )
        {
            filesystem::path p = filesystem::weakly_canonical( argv[0] );
            programName = p.string();
            programDir = p.parent_path();

            // save arg0
            optionVars [ "$0" ] = programName;
            programArgsCount++;
        }


        // Loop vars
        string previousOptName;
        int n = 1;  // skip arg0 program's name
        smatch m;

        while ( n < argc )
        {
            // Read next argument
            string arg = argv[ n++ ];

            // Catch syntax -x or --xyz option name
            if ( regex_match( arg, m, OPT_NAME_SHORT ) || regex_match( arg, m, OPT_NAME_LONG ) )
            {
                // is there a previous pending option name? insert it as missing value
                if ( !previousOptName.empty() )
                {
                    optionVars[ previousOptName ] = MISSING_VAL_TAG;
                    previousOptName.clear();
                }

                // Translate short into long name (when possible)
                if ( m[1].length() == 1 )
                {
                    string longName = longConfigOptionFor( m[1] );
                    previousOptName = !longName.empty() ? longName : m[1];
                }
                else
                {
                    previousOptName = m[1];
                }
            }

            // Catch syntax -x=VALUE or -xVALUE pair
            else if ( regex_match( arg, m, OPT_SHORT_WITH_VAL) )
            {
                // is there a previous pending option name, insert it as missing value
                if ( !previousOptName.empty() )
                {
                    optionVars[ previousOptName ] = MISSING_VAL_TAG;
                    previousOptName.clear();
                }

                // Translate short into long name (when possible)
                string longName = longConfigOptionFor( m[1] );

                if ( !longName.empty() )
                    optionVars[ longName ] = m[2];
                else
                    optionVars[ m[1] ] = m[2];
            }

            // Catch syntax --xyz=VALUE pair
            else if ( regex_match( arg, m, OPT_LONG_WITH_VAL ) )
            {
                // is there a previous pending option name?  insert it as missing value
                if ( !previousOptName.empty() )
                {
                    optionVars[ previousOptName ] = MISSING_VAL_TAG;
                    previousOptName.clear();
                }

                optionVars[ m[1] ] = m[2];
            }

            // Catch alternating name/value floating arguments
            else
            {
                // is there a previous pending option name? insert it with the new floating value
                if ( !previousOptName.empty() )
                {
                    optionVars[ previousOptName ] = arg;
                    previousOptName.clear();
                }
                else
                {
                    if ( !programArgs.empty() )
                        programArgs += " ";

                    programArgs += arg;
                    optionVars[ "$" + to_string( programArgsCount++) ] = arg;
                }

            } // if-else-chain

        }  // while


        // Is there a previous pending option name?   insert it as missing value
        if ( !previousOptName.empty())
            optionVars[ previousOptName ] = MISSING_VAL_TAG;


        // Save unnamed args, if any
        optionVars[ "$*" ] = programArgs;
        optionVars[ "$#" ] = to_string( programArgsCount );


        // Debug print options
        #if LOGGER_ENABLED
        for ( const auto &pair: optionVars )
            logi( "      option args: %s='%s'", pair.first.c_str(), pair.second.c_str());
        #endif

        return validateConfigOptions( useStrictCheck );
    }


    string loadConfigFile( const string& pathname, bool useStrictCheck )
    {
        static const regex PROPERTY_LINE( "^ *([^#=]+)=(.*)$" ); // ignore #commented lines
        static const regex COMMENT_LINE( "^ *#.*$" );            // detect #commented lines
        vector<string> lines;
        smatch m;

        // read lines
        try
        {
            lines = Strings::getFileAsLines(pathname);
        }
        catch ( const exception &e )
        {
            throw IOException( string("loadConfigFile(): ") + e.what() );
        }

        // parse and load optionVars
        string name, value;
        bool  isMultiline = false;
        for ( const string &line : lines )
        {
            if ( isMultiline )
            {
                // Keeps appending to value any line that ends with '\'
                if ( regex_match(line, COMMENT_LINE) )
                {
                    // A comment line breaks the chain!
                    optionVars[name] = value;
                    isMultiline = false;
                }
                else
                {
                    value.append( Strings::trim( line ));

                    if ( Strings::endsWith( value, "\\" ))
                    {
                        value.pop_back();  // remove \ at the end
                    }
                    else
                    { 
                        optionVars[name] = value;
                        isMultiline = false;
                    }
                }
            }
            else if ( regex_match( line, m, PROPERTY_LINE ) )
            {
                name = m[1];
                value = Strings::trim( m[2] );

                if ( Strings::endsWith( value, "\\") )
                {
                    isMultiline = true;
                    value.pop_back(); // remove \ at the end
                }
                else
                {
                    optionVars[name] = value;
                }
            }
        }

        return validateConfigOptions( useStrictCheck );
    }


    void loadConfigEnv( char **env )
    {
        static const regex EQUAL_SPLITTER( "^([^=]+)=(.*)$" );

        // Read env name/value pairs
        for ( char **pair = env; *pair != 0; pair++ )
        {
            cmatch m;
            if ( regex_match( *pair, m, EQUAL_SPLITTER ) )
                envVars.insert( std::make_pair( m[1], m[2]) );
        }
    }

    string getUser()
    {
        return get("USER");
    }

    string getUserHome()
    {
        return get("HOME");
    }


    string getProgram()
    {
        return programName;
    }

    string getProgramDir()
    {
        return programDir;
    }

    int getArgsCount()
    {
        return programArgsCount;
    }

    string getArg( int pos, const string& defVal )
    {
        if ( pos < 0 || pos >= programArgsCount )
            return defVal;

        auto it = optionVars.find( "$" + to_string(pos) );
        return (it == optionVars.end()) ? defVal : it->second;
    }

    bool getArgBool( int pos )
    {
        return Strings::parseBool( getArg(pos) );
    }

    int getArgInt( int pos, int defVal )
    {
        return Strings::parseInt( getArg(pos), defVal );
    }

    long getArgLong( int pos, long defVal )
    {
        return Strings::parseLong( getArg(pos), defVal );
    }

    double getArgDouble( int pos, double defVal )
    {
        return Strings::parseDouble( getArg(pos), defVal );
    }


    bool hasOption( const string &name )
    {
        auto it = optionVars.find( name );
        if ( it != optionVars.end() )
            return true;

        auto it2 = envVars.find( name );
        return it2 != envVars.end();

    }


    bool remove( const string& name )
    {
        size_t n = 0;
        n += optionVars.erase( name );
        n += envVars.erase( name );

        return n > 0;
    }


    string get( const string& name, const string& defVal )
    {
        auto it = optionVars.find( name );
        if ( it != optionVars.end() )
            return it->second;

        auto it2 = envVars.find( name );
        return (it2 == envVars.end()) ? defVal : it2->second;
    }

    bool getBool( const string& name )
    {
        return Strings::parseBool( get(name) );
    }

    int getInt( const string& name, int defVal )
    {
        return Strings::parseInt( get(name), defVal );
    }

    long getLong( const string& name, long defVal )
    {
        return Strings::parseLong( get(name), defVal );
    }

    double getDouble( const string& name, double defVal )
    {
        return Strings::parseDouble( get(name), defVal );
    }


    void set( const string& name, const string& value )
    {
        optionVars[ name ] = value;
    }

    void setBool( const string& name, bool value )
    {
        optionVars[name] = value ? "true" : "false";
    }

    void setInt( const string& name, int value )
    {
        optionVars[ name ] = to_string( value);
    }

    void setLong ( const string& name, long value )
    {
        optionVars[ name ] = to_string( value);
    }

    void setDouble( const string& name, double value )
    {
        optionVars[ name ] = to_string( value);
    }


    vector<string> getNames( bool includeEnvVars )
    {
        vector<string> v;

        if ( includeEnvVars )
        {
            for ( const auto &pair: envVars )
                v.push_back( pair.first );
        }

        for( const auto& pair : optionVars )
            v.push_back( pair.first );


        return v;
    }


    string toString( bool includeEnvVars )
    {
        stringstream ss;

        if ( includeEnvVars )
        {
            for ( const auto &pair: envVars )
                ss << pair.first << "=" << pair.second << endl;
        }

        ss << "@User"             << "=" << getUser() << endl;
        ss << "@UserHome"         << "=" << getUserHome() << endl;
        ss << "@Program"          << "=" << getProgram() << endl;
        ss << "@ProgramDir"       << "=" << getProgramDir() << endl;
        ss << "@ProgramArgsCount" << "=" << programArgsCount << endl;
        ss << "@ProgramArgs"      << "=" << programArgs << endl;

        for( const auto& pair : optionVars )
            ss << pair.first << "=" << pair.second << endl;

        return ss.str();
    }



    string getOptionsHelp()
    {
        const int INDENT = 30;
        stringstream ss;

        // Iterate through the known options
        for( const auto& o : configOptions )
        {
            // Append documentation lines
            int n = 0;
            vector<string> doclines = Strings::split( o.docstr, "\n" );

            for ( const string& line : doclines )
            {
                if ( n++ == 0 )
                    ss << std::left << std::setw(INDENT) << o.optionNames();
                else
                    ss << std::left << std::setw(INDENT) << " ";

                ss << " " << line << endl;
            }

            // Append required/optional remarks
            ss << std::left << std::setw(INDENT) << " " << " ";

            if ( o.isRequired )
            {
                 ss << "Required";
            }
            else
            {
                if ( o.type == STRING )
                    ss <<  "Defaults to \"" << o.defVal << "\"" ;
                else
                    ss <<  "Defaults to " << o.defVal;
            }

            ss << endl << endl;


        } // for

        return ss.str();
    }


} // ns

