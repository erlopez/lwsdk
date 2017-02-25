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
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <utility>

namespace lwsdk::Config
{
    enum ConfigOptionType { BOOL, STRING, UINT, INT, DECIMAL };

    /**
     * Clear all loaded configuration and environmental values.
     */
    void reset();

    /**
     * Clear all defined configuration options.
     */
    void resetDefinitions();

    /**
     * Defines optional configuration option for the application
     *
     * @param type        value type, see ConfigOptionType enum
     * @param longName    configuration long name, used in config file or args
     * @param defVal      default value if none is specified in config file or args
     * @param docstr      text description of this configuration option
     */
    void defineConfigOption( ConfigOptionType type, const std::string &longName, const std::string &defVal, const std::string &docstr );


    /**
     * Defines required configuration option for the application
     *
     * @param type        value type, see ConfigOptionType enum
     * @param longName    configuration long name, used in config file or args
     * @param docstr      text description of this configuration option
     */
    void defineConfigOption( ConfigOptionType type, const std::string &longName, const std::string &docstr );

    /**
     * Defines optional configuration option for the application
     *
     * @param type        value type, see ConfigOptionType enum
     * @param shortName   configuration short name, used in config args
     * @param longName    configuration long name, used in config file or args
     * @param defVal      default value if none is specified in config file or args
     * @param docstr      text description of this configuration option
     */
    void defineConfigOption( ConfigOptionType type, char shortName, const std::string &longName, const std::string &defVal, const std::string &docstr );

    /**
     * Defines required configuration option for the application
     *
     * @param type        value type, see ConfigOptionType enum
     * @param shortName   configuration short name, used in config args
     * @param longName    configuration long name, used in config file or args
     * @param docstr      text description of this configuration option
     */
    void defineConfigOption( ConfigOptionType type, char shortName, const std::string &longName, const std::string &docstr );


    /**
     * Load environment variables
     */
    void loadConfigEnv( char **env );

    /**
     * Load configuration optionVars from the given pathname top a file.
     * The file must have a name=value pair per line.
     * Lines beginning with # are ignored comments.
     * 
     * Returns an empty string if all configuration options are valid,
     * otherwise returns an error message. This function uses the rules provided
     * by defineConfigOption().
     *
     * @throw IOException if an error occurs.
     */
    std::string loadConfigFile( const std::string& pathname, bool useStrictCheck = false );

    /**
     * Load command line arguments
     * Returns an empty string if all configuration options are valid,
     * otherwise returns an error message. This function uses the rules provided
     * by defineConfigOption().
     */
    std::string loadConfigArgs( int argc, char **argv, bool useStrictCheck = false );


    /**
     * Return current's user name. This is derived from the $USER shell environment
     * variable.
     */
    std::string getUser();

    /**
     * Return the path to the current's user directory. This is derived from the
     * $HOME shell environment variable.
     */
    std::string getUserHome();


    /**
     * Returns the absolute path to the current program
     */
    std::string getProgram();

    /**
     * Returns the path to the directory containing the current program
     */
    std::string getProgramDir();

    /**
     * Return the number of floating arguments given to the program.
     * This excludes named arguments -x or --xyz and their respective values.
     */
    int getArgCount();

    /**
     * Return floating argument at given position. Note named arguments such as
     * -x or --xyz and their respective values are not considered floating arguments.
     * For example in this command:
     *       ./program -x 1 dummy --foo=bar --fruit apple orange grape
     * the floating arguments are:
     *       [0] ./program
     *       [1] "dummy"
     *       [2] "orange"
     *       [3] "grape"
     * The named values are: x=1, foo=bar, fruit=apple
     *
     * @param pos A number from 0 to getArgsCount()-1. Any other value will
     *            return defVal. Pos=0 refers to the program's name.
     */
    std::string getArg( int pos, const std::string& defVal = "" );


    /**
     * Returns true is the argument at the given position is either
     * "true", "enable[d]", "y", "yes", or "1" (case insensitive),
     * otherwise returns false.
     */
    bool getArgBool( int pos );

    /**
     * Returns the argument at the given position converted to int. If the argument
     * cannot be converted, or the given position is out of range, default value is
     * returned.
     */
    int getArgInt( int pos, int defVal );

    /**
     * Returns the argument at the given position converted to long. If the argument
     * cannot be converted, or the given position is out of range, default value is
     * returned.
     */
    long getArgLong( int pos, long defVal );

    /**
     * Returns the argument at the given position converted to double. If the argument
     * cannot be converted, or the given position is out of range, default value is
     * returned.
     */
    double getArgDouble( int pos, double defVal );


    /**
     * Test if the configuration has an option with the given name.
     */
    bool hasOption( const std::string& name );

    /**
     * Removes the given property name from the configuration.
     * Returns true if the property was found and removed, false if the
     * property was not found.
     */
    bool remove( const std::string& name );

    /**
     * Returns the value for the given property name. If the property
     * value does not exist, the given default value is returned.
     */
    std::string get( const std::string& name, const std::string& defVal = "" );


    /**
     * Returns true is the value for the given option name is either
     * "true", "enable[d]", "y", "yes", or "1" (case insensitive),
     * otherwise returns false.
     */
    bool getBool( const std::string& name );

    /**
     * Returns the value for the given property name converted to int. If the property
     * value does not exist or cannot be converted, the given default value is returned.
     */
    int getInt( const std::string& name, int defVal );

    /**
     * Returns the value for the given property name converted to long. If the property
     * value does not exist or cannot be converted, the given default value is returned.
     */
    long getLong( const std::string& name, long defVal );

    /**
     * Returns the value for the given property name converted to double. If the property
     * value does not exist or cannot be converted, the given default value is returned.
     */
    double getDouble( const std::string& name, double defVal ) ;


    /**
     * Sets or overwrite the value of the given property name.
     */
    void set( const std::string& name, const std::string& value );

    /**
     * Sets or overwrite the value of the given property name.
     */
    void setBool( const std::string& name, bool value );

    /**
     * Sets or overwrite the value of the given property name.
     */
    void setInt( const std::string& name, int value );

    /**
     * Sets or overwrite the value of the given property name.
     */
    void setLong ( const std::string& name, long value );

    /**
     * Sets or overwrite the value of the given property name.
     */
    void setDouble( const std::string& name, double value );




    /**
     * Returns all property names stored in this configuration
     */
    std::vector<std::string> getNames( bool includeEnvVars = true );

    /**
     * Returns the configuration variables as a multiline string
     */
    std::string toString( bool includeEnvVars = true );


    /**
     * Returns usage help based on defined options
     */
    std::string getOptionsHelp();
    
} // ns

#endif //CONFIG_H
