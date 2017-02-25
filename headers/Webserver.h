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
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <functional>
#include "Result.h"

namespace lwsdk::Webserver
{
    /**
     * Holds information about a websocket message
     */
    struct WSMessage
    {
        uint32_t    connectionId;  // ID of the connection the message is associated with
        std::string msg;           // message contents

        WSMessage( uint32_t connectionId, std::string msg ) :
               connectionId( connectionId ), msg(std::move( msg )) {}
    };
    
    /**
     * User callback to receive web socket messages
     * @param connectionId ID of the connection the message was received from
     * @param message      The message contents
     */
    typedef std::function<void( uint32_t connectionId, const std::string& message )>
            MessageCallback_t;


    /**
     * Registers an user callback function to receive incoming websocket messages.
     * @param msgCallback Pointer to user defined function. May be set to null to
     *                    remove any previously set function.
     *
     */
    void setMessageCallback( const MessageCallback_t&  msgCallback );

    /**
     * Configure the web server. This function must be called before starting the web server.
     * At least one valid port kind (http/https) must be specified.
     *
     * @param hostname  Name of the server: 'localhost', 'example.com', etc.
     * @param webDir    Path to the directory containing the static web content such as
     *                  index.html and 404.html.
     * @param port      Port to listen for non-secure connections: 80, 8080, etc. Set to -1 to
     *                  disable http but must call setConfigSSL() to specify a https port later.
     *
     * @throw RuntimeException if the given configuration is invalid or the web server is currently
     *        running.
     */
    void setConfig( std::string hostname, std::string webDir, int port = -1);


    /**
     * Enable SSL port on web server. This is required if the http port was disabled when calling
     * setConfig(), or could be called in addition to enable both http and https.
     *
     * A self-signed cert/key file pair can be created with:
     * $ openssl req -new -newkey rsa:1024 -days 10000 -nodes -x509 -subj "/C=MyCountry/ST=MyState/L=MyCity/O=MyCompany/CN=MyDomain" -keyout "my.key" -out "my.cert"
     *
     * @param portTls       Port to listen for secure connections: 443, 8443, etc.
     * @param sslCertPath   Path to the cert file
     * @param sslKeyPath    Path to the key file
     *
     * @throw RuntimeException if the given configuration is invalid or the web server is currently
     *        running.
     */
    void setConfigSSL( int portTls, std::string sslCertPath, std::string sslKeyPath );

    /**
     * Returns a multiline string with the current configuration.
     */
    std::string getConfig();

    /**
     * Returns the currently configured hostname
     */
    std::string getHostname();

    /**
     * Returns the currently configured web directory
     */
    std::string getWebDir();

    /**
     * Returns the currently configured http port
     */
    int getPort();

    /**
     * Returns the currently configured https port
     */
    int getSSLPort();

    /**
     * Returns the currently configured SSL cert file path
     */
    std::string getSSLCertPath();

    /**
     * Returns the currently configured SSL key file path
     */
    std::string getSSLKeyPath();


    /**
     * Test if the web server is running.
     */
    bool isRunning();

    /**
     * Starts the web server with the current configuration.
     * If the server is already running, this function does nothing.
     *
     * @throw RuntimeException if the current configuration is invalid.
     */
    void start();

    /**
     * Stops the web server. If the server is not running, this function
     * does nothing.
     */
    void stop();

    /**
     * Return the number of connected web socket clients.
     */
    int getClientCount();

    /**
     * Enqueue a message for delivery.
     * @param message The message contents.
     * @param destId  If different than 0, the message is delivered only to the
     *                web connection identified by the given ID, otherwise
     *                delivered to all active web clients.
     * @return true if the message was enqueued for delivery, false if the outgoing
     *         queue is full and no more messages can be accepted at this time.
     */
    bool sendMessage( const std::string& message, uint32_t destId = 0 );

    /**
     * Retrieve a message from the incoming queue, waiting up to a maximum of timeoutMsec
     * if not message is available.
     *
     * NOTE: If a MessageCallback has been specified, this function always returns an
     *       empty value. Incoming messages are delivered via callback function only.
     *
     * Example for processing returned optional value:
     *
     *        auto optMsg = receiveMessage(100);
     *        if ( optMsg.has_value() )
     *           doSomething( optMsg.value() );
     *
     * @param timeoutMsec Maximum number of milliseconds to wait for new message. Set this
     *                    value to 0 for non-blocking behaviour.
     * @return A new message if any is available, otherwise an empty value if the operation
     *         timed out.
     */
    Result<WSMessage> receiveMessage( uint32_t timeoutMsec );


} // namespace

#endif //WEBSERVER_H
