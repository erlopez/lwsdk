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
#include <map>
#include <sstream>
#include <utility>
#include <mutex>
#include <thread>
#include <optional>

#include "Webserver.h"
#include "Files.h"
#include "Exceptions.h"
#include "Utils.h"
#include "ConcurrentQueue.h"

#include <libwebsockets.h>

#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"

#define PRETTY_FUNC std::string(__PRETTY_FUNCTION__)

using namespace std;

namespace lwsdk::Webserver
{
    #define MAX_PAYLOAD 4096                          // Size of the buffers used to serialize data in and out of the web socket
    #define MAX_CLIENTS 64                            // Maximum number of connections accepted by this server

    // Forwards
    static void interrupt();
    static int lwsCallback( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len );
    static void mainServerThread();
    static const char * asString( int n );

    // Website config vars
    static string hostname;                           // Virtual host name
    static int    port = -1;                          // http port,  < 0 means no http server is started
    static string webDir;                             // Directory containing web static files such index.html
    static string sslCertPath;                        // SSL/TLS cert file
    static string sslKeyPath;                         // SSL/TLS key file
    static int    sslPort = -1;                       // https port, < 0 means no https server is started


    // Website state machine vars
    static atomic_bool       keepWorking = false;             // Server running state
    static thread            *serverThread = nullptr;         // Server thread main loop function
    static thread            *msgDispatcherThread = nullptr;  // Thread function used to dispatch incoming websocket messages
    static MessageCallback_t userCallback = nullptr;          // Pointer to the user-defined callback function, null if none


    // Holds information and data-transfer state of a websocket connection
    struct WSConnection {
        uint32_t id;                                  // connection's id
        struct lws * wsi;                             // underlying LWS connection handle
        string inbox;                                 // string to store incoming data
        shared_ptr<string> outbox{};                  // string holding outgoing data
        int outpos{0};                                // index, points to the beginning of the next chunk of data being written out, ie. &outbox[outpos]
        char outbuf[ LWS_PRE + MAX_PAYLOAD ]{0};      // buffer holding data being written out, with spare LWS_PRE-sized space
        WSConnection( uint32_t id, struct lws *wsi ) : id( id ), wsi( wsi ) {}
    };


    // Message queues, connections list
    static uint32_t connectionIdSeq = 1;                     // Incremental sequence to generate new connection IDs
    static map<uint32_t, WSConnection> wsConnections;        // Stores active connections, indexed by ID
    static mutex wsConnectionsMutex;                         // Mutex to provide thread-safe access to wsConnections[] map
    static ConcurrentQueue<WSMessage> incomingMessages(100); // Hold messages coming from web clients
    static ConcurrentQueue<WSMessage> outgoingMessages(100); // Hold messages going out to web clients


    // LWS config boilerplate structures
    struct lws_context       *context = nullptr;             // LWS web context, needed to call all LWS apis

    struct per_session_data {                                // Structure allocated automatically by LWS for every connection
        WSConnection*  connection;
    };

    static struct lws_protocols protocols[] = {
        { "http",  lws_callback_http_dummy, 0, 0, 0, nullptr, 0 },                           // first protocol must always be HTTP handler
        { "ws0",   lwsCallback, sizeof(struct per_session_data), MAX_PAYLOAD, 0, nullptr },  // websocket protocol
        { nullptr, nullptr,  0 /* End of list */ }
    };

    static const struct lws_extension exts[] = {
        { "permessage-deflate",  lws_extension_callback_pm_deflate,  "permessage-deflate; client_no_context_takeover; client_max_window_bits" },
        { "deflate-frame", lws_extension_callback_pm_deflate,  "deflate_frame" },
        { nullptr, nullptr, nullptr /* terminator */}
    };

    // Extra mime types
    static const struct lws_protocol_vhost_options pvo_mime_sh  = { NULL,          NULL, ".sh",  "application/x-sh" };
    static const struct lws_protocol_vhost_options pvo_mime_csv = { &pvo_mime_sh,  NULL, ".csv", "text/csv" };
    static const struct lws_protocol_vhost_options pvo_mime_gz  = { &pvo_mime_csv, NULL, ".gz",  "application/gzip" };
    static const struct lws_protocol_vhost_options pvo_mime_tgz = { &pvo_mime_gz,  NULL, ".tgz", "application/gzip" };
    static const struct lws_protocol_vhost_options pvo_mime_zip = { &pvo_mime_tgz, NULL, ".zip", "application/zip" };
    static const struct lws_protocol_vhost_options pvo_mime_pdf = { &pvo_mime_zip, NULL, ".pdf", "application/pdf" };

    static struct lws_http_mount mount = {
         .mount_next =            nullptr,       // linked-list "next"
         .mountpoint =            "/",           // mount point URL
         .origin =                nullptr,       // static content wed dir, set below when starting the server
         .def =                   "index.html",  // default filename
         .protocol =              nullptr,
         .cgienv =                nullptr,
         .extra_mimetypes =       &pvo_mime_pdf,
         .interpret =             nullptr,
         .cgi_timeout =           0,
         .cache_max_age =         0,
         .auth_mask =             0,
         .cache_reusable =        0,
         .cache_revalidate =      0,
         .cache_intermediaries =  0,
         .origin_protocol =       LWSMPRO_FILE,
         .mountpoint_len =        1,             // char count
         .basic_auth_login_file = nullptr,
    };


    //
    // ================================================ Public API ===========================================================
    //

    void setConfig( std::string hostname, std::string webDir, int port )
    {
        if ( keepWorking )
            throw RuntimeException( PRETTY_FUNC + " - Cannot change config while web server is running." );

        Webserver::hostname = hostname;
        Webserver::webDir = webDir;
        Webserver::port = port;

    }

    void setConfigSSL( int sslPort, std::string sslCertPath, std::string sslKeyPath )
    {
        if ( keepWorking )
            throw RuntimeException( PRETTY_FUNC + " - Cannot change config while web server is running." );

        if ( !Files::exists(sslCertPath) )
            throw RuntimeException( PRETTY_FUNC + " - Cannot find sslCertPath file: " + sslCertPath );

        if ( !Files::exists(sslKeyPath) )
            throw RuntimeException( PRETTY_FUNC + " - Cannot find sslCertPath file: " + sslKeyPath );

        Webserver::sslKeyPath = sslKeyPath;
        Webserver::sslCertPath = sslCertPath;
        Webserver::sslPort = sslPort;
    }


    std::string getConfig()
    {
        ostringstream ss;

        ss << "Hostname:      " << hostname << endl;
        ss << "Web directory: " << webDir << endl;
        ss << "HTTP port:     " << port << endl;
        ss << "HTTP enabled:  " << (port > 0 ? "true" : "false") << endl;
        ss << "SSL port:      " << sslPort << endl;
        ss << "SSL Cert Path: " << sslCertPath << endl;
        ss << "SSL Key Path:  " << sslKeyPath << endl;
        ss << "HTTPS enabled: " << (sslPort > 0 ? "true" : "false") << endl;

        return ss.str();
    }

    std::string getHostname()
    {
        return hostname;
    }

    std::string getWebDir()
    {
        return webDir;
    }

    int getPort()
    {
        return port;
    }

    int getSSLPort()
    {
        return sslPort;
    }

    std::string getSSLCertPath()
    {
        return sslCertPath;
    }

    std::string getSSLKeyPath()
    {
        return sslKeyPath;
    }

    void setMessageCallback( const MessageCallback_t& msgCallback )
    {
        userCallback = msgCallback;
    }

    bool sendMessage( const std::string& message, uint32_t destId )
    {
        WSMessage m( destId, message );
        bool res = outgoingMessages.offer( m, 0 );

        if (res)
            interrupt(); // wake lws_service function

        return res;
    }

    std::optional<WSMessage> receiveMessage( uint32_t timeoutMsec )
    {
        return userCallback ? nullopt : incomingMessages.take( timeoutMsec );
    }

    int getClientCount()
    {
        lock_guard lock( wsConnectionsMutex );
        return (int) wsConnections.size();
    }

    bool isRunning()
    {
        return keepWorking;
    }


    void start()
    {
        if ( serverThread != nullptr )
            return;

        if ( port == sslPort )
            throw RuntimeException( PRETTY_FUNC + " - port and sslPort cannot be the same." );

        if ( port <= 0 && sslPort <= 0 )
            throw RuntimeException( PRETTY_FUNC + " - At least one port must be valid." );

        if ( !Files::exists(webDir) )
            throw RuntimeException( PRETTY_FUNC + " - Invalid web directory: " + webDir );

        keepWorking = true;
        serverThread = new thread( mainServerThread );
    }


    void stop()
    {
        if ( serverThread == nullptr )
            return;

        // tell main thread loop to exit
        keepWorking = false;
        interrupt();

        // Wait for thread to finish
        serverThread->join();

        // Cleanup
        delete serverThread;
        serverThread = nullptr;
    }


    //
    // ================================================ Private API ===========================================================
    //

    /**
     * Wake the lws service loop if blocked on IO.
     */
    static void interrupt()
    {
        if ( context != nullptr )
            lws_cancel_service( context );
    }


    /**
     * Adds a connection data object to the list.
     * @return pointer to added structure, null if the maximum number
     *         of connections is reached.
     */
    static WSConnection* addConnection( struct lws * wsi )
    {
        lock_guard lock( wsConnectionsMutex );

        if ( wsConnections.size() >= MAX_CLIENTS )
        {
            loge( "MAX_CLIENTS reached." );
            return nullptr;
        }

        // Ensure sequence is never 0 if it wraps around
        if ( connectionIdSeq == 0)
            connectionIdSeq++;

        uint32_t id = connectionIdSeq++;   // ok if rolls over

        wsConnections.emplace( make_pair( id, WSConnection( id, wsi)) );

        return &wsConnections.find( id )->second;

    }

    /**
     * Find a connection object by id
     * @return Pointer to the requested connection, null if the given
     *         id is not found in the list.
     */
    static WSConnection* getConnection( uint32_t id )
    {
        lock_guard lock( wsConnectionsMutex );

        auto it = wsConnections.find( id );
        if ( it != wsConnections.end() )
            return &it->second;

        return nullptr;
    }

    /**
     * Removes the connection associated with the given id from the list.
     * @return true if the id was found and removed, false otherwise.
     */
    static bool removeConnection( uint32_t id )
    {
        lock_guard lock( wsConnectionsMutex );
        return wsConnections.erase( id ) > 0;
    }


    /**
     * Main dispatcher thread loop to deliver received messages to user callback
     */
    static void mainDispatcherThread()
    {
        logw( "Dispatcher thread stating ..." );
        while ( keepWorking && userCallback )
        {
            try
            {
                // block until new message arrives
                WSMessage m = incomingMessages.take();

                try
                {
                    // Invoke user callback
                    userCallback( m.connectionId, m.msg );
                }
                catch ( const std::exception& e2 )
                {
                    loge( "User callback finished with errors - %s ", e2.what() );
                }

            }
            catch ( const InterruptedException& e )
            {
                logw( "Dispatcher thread was interrupted! " );
            }

        } // while

        logw( "Dispatcher thread exited." );
    }

    /**
     * Main service thread loop for web server
     */
    static void mainServerThread()
    {
        logw( "Web server thread started ..." );

        struct lws_context_creation_info info{};

        try
        {
            // Set what debug level to emit and to send it to syslog
            int logs = LLL_USER | LLL_ERR | LLL_WARN /*| LLL_NOTICE*/
            /* for LLL_ verbosity above NOTICE to be built into lws,
             * lws must have been configured and built with
             * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
            /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
            /* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
            /* | LLL_DEBUG */;

            lws_set_log_level( logs, nullptr );

            // Set origin dir, the location of web documents, note that
            // the char *pointer returned by data() is OK as the server configuration is not
            // allowed to change while the server is running. Also since C++11 data() is null terminated.
            mount.origin = webDir.data();

            // Create context
            memset( &info, 0, sizeof info );
            info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS  // only creates the context without default vhost, manually added below
                           // | LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;  // page can only include same domain resources
                           ;

            context = lws_create_context( &info );
            if ( !context )
                throw RuntimeException( PRETTY_FUNC + " - libwebsocket context init failed." );

            // Populate info structure to  create virtual hosts
            // Common config for all hosts
            //info.iface = "eth0";     // bond to specific adapter, otherwise all e.g. "eth1" "eth2" "wifi0"
            info.gid = -1;             // group id to change to after setting listen socket, or -1.
            info.uid = -1;             // user id to change to after setting listen socket, or -1.
            info.error_document_404 = "/404.html";

            // Websocket config for all hosts
            info.protocols = protocols;
            info.options |= LWS_SERVER_OPTION_VALIDATE_UTF8;
            //info.extensions = exts;   // deflate websockets extensions to support compressed streams

            // Setup http host
            if ( port > 0 )
            {
                info.vhost_name = hostname.data();
                info.port = port;
                info.mounts = &mount;

                if ( !lws_create_vhost( context, &info ))
                    throw RuntimeException( PRETTY_FUNC + " - libwebsocket failed to create http vhost." );

            }


            // Setup https host
            if ( sslPort > 0 )
            {
                info.vhost_name = hostname.data();
                info.port = sslPort;
                info.mounts = &mount;
                info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
                info.ssl_cert_filepath = sslCertPath.data();
                info.ssl_private_key_filepath = sslKeyPath.data();

                if ( !lws_create_vhost( context, &info ) )
                    throw RuntimeException( PRETTY_FUNC + " - libwebsocket failed to create https vhost." );

            }

            // If there is a user callback, start dispatcher thread
            if ( userCallback )
            {
                logw( "Web server thread starting message dispatcher thread ..." );
                msgDispatcherThread = new thread( mainDispatcherThread );
            }

            // Thread Main loop
            int n = 0;
            
            while ( n >= 0 && keepWorking )
            {
                n = lws_service( context, 0 );


                #if LOGGER_ENABLED
                putchar( 'o' );
                fflush( stdout );
                #endif

                // Copy outgoing messages to their respective connection
                // output buffers, if any
                while ( outgoingMessages.size() > 0 )
                {
                    WSMessage m = outgoingMessages.take();
                    shared_ptr<string> msg = std::make_shared<string>(m.msg);

                    for ( auto& conn : wsConnections )
                    {
                        if ( conn.second.outbox ) // still sending?
                        {
                            loge( "Connection %u busy sending; skipping..", conn.first );
                            continue;
                        }

                        if ( m.connectionId == 0 || m.connectionId == conn.first )
                        {
                            conn.second.outbox = msg;
                            conn.second.outpos = 0;
                            lws_callback_on_writable( conn.second.wsi ); // schedule a lws_callback to write

                            // Flush pending callback writes
                            n = lws_service( context, 0 );
                        }

                    } // for

                } // while

            }

        }
        catch ( const std::exception& e )
        {
            loge( "Web server thread error: %s",  e.what() );

        }

        logw( "Web server thread freeing context ..." );
        if ( context != nullptr )
            lws_context_destroy( context );

        context = nullptr;
        keepWorking = false;


        // Wait for dispatcher thread to exit, if any
        if ( msgDispatcherThread != nullptr )
        {
            logw( "Web server thread waiting on dispatcher exit ..." );
            incomingMessages.interrupt(); // wake dispatcher thread if blocked waiting for messages
            msgDispatcherThread->join();
            delete msgDispatcherThread;
            msgDispatcherThread = nullptr;
        }

        logw( "Web server thread stopped." );
    }


    /**
     * Handle Webserver and Websocket events
     * see https://libwebsockets.org/lws-api-doc-main/html/group__usercb.html#gad62860e19975ba4c4af401c3cdb6abf7
     */
    static int lwsCallback( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
    {
        struct per_session_data *pss = (struct per_session_data *) user;
        struct lws_vhost *vhost = lws_get_vhost(wsi);
        const struct lws_protocols *protocol = lws_get_protocol(wsi);

        //loge("protocol name --> %s, vhost port --> %d",  protocol->name , lws_get_vhost_listen_port(vhost) );


        //struct vhd_minimal_server_echo *vhd = (struct vhd_minimal_server_echo *) lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));


        logB( "[%d] %s", reason, asString(reason) );

        switch ( reason )
        {
            // A websocket client connected, add it from our list
            case LWS_CALLBACK_ESTABLISHED:
            {
                logi( "LWS_CALLBACK_ESTABLISHED" );
                pss->connection = addConnection( wsi );

                if ( pss->connection == nullptr )
                    return -1;


                logw( "\nclients=%d", getClientCount());

                break;
            }

           // A websocket client disconnected, remove it from our list
           case LWS_CALLBACK_CLOSED:
           {
               logi( "LWS_CALLBACK_PROTOCOL_DESTROY");

               if ( pss->connection == nullptr )
                    return -1;

               removeConnection( pss->connection->id );
               logw("\nclients=%d", getClientCount() );

               break;
           }

            // A web client sent us data, it might take several calls
            // to collect all the MAX_PAYLOAD-sized chunks making up the whole message
            case LWS_CALLBACK_RECEIVE:
            {
                logw( "LWS_CALLBACK_RECEIVE" );

                if ( pss->connection == nullptr )
                    return -1;

                WSConnection* connection = pss->connection;

                int first = lws_is_first_fragment( wsi );
                int final = lws_is_final_fragment( wsi );
                int binary = lws_frame_is_binary( wsi );

                logw( FG10 "Data: RX len=%ld first=%ld, final=%ld, binary=%ld",
                      (long) len, (long) first, (long) final, (long) binary );

                #if LOGGER_ENABLED
                Utils::memdump( in, len );
                #endif
                

                if ( first )
                    connection->inbox.clear();

                connection->inbox.append( (char *) in, (long) len );

                if ( final )
                {
                    WSMessage msg( connection->id, connection->inbox );
                    if ( !incomingMessages.offer( msg, 0 ))
                    {
                        loge( "Incoming queue full; discarding incoming message: '%s'",
                              connection->inbox.c_str() );
                    }
                }

                //lws_callback_on_writable( wsi );
                //lws_callback_on_writable_all_protocol( context, &protocols[1] );              // a) --or--
                //lws_callback_on_writable_all_protocol( lws_get_context(wsi), &protocols[1] );   // b)

                break;
            }

            // Application is sending data to a given websocket client
            case LWS_CALLBACK_SERVER_WRITEABLE:
            {
                logw( "LWS_CALLBACK_SERVER_WRITEABLE" );

                if ( pss->connection == nullptr )
                    return -1;

                WSConnection* connection = pss->connection;
                if ( !connection->outbox  ) // spurious write callback? ignore
                    return 0;

                string &s = *connection->outbox;
                char* buf = connection->outbuf;   // formatted as [LWS_PRE:DATA_BUFFER]

                // Compute start pos and byte count of the next chunk of text to send
                // from the outbox string
                int start = connection->outpos;                            // outbox's chunk start
                int end = std::min( (int)s.size(), start + MAX_PAYLOAD );  // outbox's chunk end
                int length = end - start;                                  // chunk's length
                connection->outpos = end;                                  // save position of next chunk

                memcpy( &buf[LWS_PRE], &s[start], length );

                // Generate lws write flags
                int first = (start == 0);                                  // is first chunk?
                int final = (start + MAX_PAYLOAD >= (int)s.length() );     // is last chunk?
                int flags = lws_write_ws_flags( LWS_WRITE_TEXT, first, final);

                logi("WRITE: destId=%d, slen=%d, range[ %03d..%03d ), len=%d, first=%d, final=%d",
                             connection->id, (int)s.length(), start, end, length, first, final );

                // Write chunk from outbox's string
                // notice we allowed for LWS_PRE spare space in front of payload data
                int n = lws_write( wsi, (unsigned char *)&buf[LWS_PRE], length,
                                        (enum lws_write_protocol)flags );

                if ( n < length || final )
                    connection->outbox = nullptr; // free string

                if (n < length)
                {
                    loge("WRITE: Incomplete write error, only %d of %d written to ws socket\n", n, length );
                    return -1;
                }

                // If not done, request write for next chunk
                if ( !final )
		            lws_callback_on_writable(wsi);

                break;
            }


            case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
            {
                logw( "LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED" );
                // reject everything else except permessage-deflate
                if ( strcmp( (char*)in, "permessage-deflate" ) )
                    return 1;

                break;
            }


            default:
            {
                //            if ( reason != 31 )
                //                logw( "Unhandled reason = %d, %s", reason, asString( reason ) );
                break;
            }
        }

        return 0;
    }


    /**
     * Converts libwebsocket event number to string, for debugging purposes.
     */
    static const char * asString( int n )
    {
        switch(n)
        {
            case LWS_CALLBACK_PROTOCOL_INIT:                               return "ACK_PROTOCOL_INIT";
            case LWS_CALLBACK_PROTOCOL_DESTROY:                            return "LWS_CALLBACK_PROTOCOL_DESTROY";
            case LWS_CALLBACK_WSI_CREATE:                                  return "LWS_CALLBACK_WSI_CREATE";
            case LWS_CALLBACK_WSI_DESTROY:                                 return "LWS_CALLBACK_WSI_DESTROY";
            case LWS_CALLBACK_WSI_TX_CREDIT_GET:                           return "LWS_CALLBACK_WSI_TX_CREDIT_GET";
            case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:      return "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS";
            case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:      return "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS";
            case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:    return "LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION";
            //case LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY:        return "LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY";    // not backwards compatible?
            case LWS_CALLBACK_SSL_INFO:                                    return "LWS_CALLBACK_SSL_INFO";
            case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:    return "LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION";
            case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:              return "LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED";
            case LWS_CALLBACK_HTTP:                                        return "LWS_CALLBACK_HTTP";
            case LWS_CALLBACK_HTTP_BODY:                                   return "LWS_CALLBACK_HTTP_BODY";
            case LWS_CALLBACK_HTTP_BODY_COMPLETION:                        return "LWS_CALLBACK_HTTP_BODY_COMPLETION";
            case LWS_CALLBACK_HTTP_FILE_COMPLETION:                        return "LWS_CALLBACK_HTTP_FILE_COMPLETION";
            case LWS_CALLBACK_HTTP_WRITEABLE:                              return "LWS_CALLBACK_HTTP_WRITEABLE";
            case LWS_CALLBACK_CLOSED_HTTP:                                 return "LWS_CALLBACK_CLOSED_HTTP";
            case LWS_CALLBACK_FILTER_HTTP_CONNECTION:                      return "LWS_CALLBACK_FILTER_HTTP_CONNECTION";
            case LWS_CALLBACK_ADD_HEADERS:                                 return "LWS_CALLBACK_ADD_HEADERS";
            case LWS_CALLBACK_VERIFY_BASIC_AUTHORIZATION:                  return "LWS_CALLBACK_VERIFY_BASIC_AUTHORIZATION";
            case LWS_CALLBACK_CHECK_ACCESS_RIGHTS:                         return "LWS_CALLBACK_CHECK_ACCESS_RIGHTS";
            case LWS_CALLBACK_PROCESS_HTML:                                return "LWS_CALLBACK_PROCESS_HTML";
            case LWS_CALLBACK_HTTP_BIND_PROTOCOL:                          return "LWS_CALLBACK_HTTP_BIND_PROTOCOL";
            case LWS_CALLBACK_HTTP_DROP_PROTOCOL:                          return "LWS_CALLBACK_HTTP_DROP_PROTOCOL";
            case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE:                        return "LWS_CALLBACK_HTTP_CONFIRM_UPGRADE";
            case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:                     return "LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP";
            case LWS_CALLBACK_CLOSED_CLIENT_HTTP:                          return "LWS_CALLBACK_CLOSED_CLIENT_HTTP";
            case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:                    return "LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ";
            case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:                         return "LWS_CALLBACK_RECEIVE_CLIENT_HTTP";
            case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:                       return "LWS_CALLBACK_COMPLETED_CLIENT_HTTP";
            case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:                       return "LWS_CALLBACK_CLIENT_HTTP_WRITEABLE";
            case LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL:                   return "LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL";
            case LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL:                   return "LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL";
            case LWS_CALLBACK_ESTABLISHED:                                 return "LWS_CALLBACK_ESTABLISHED";
            case LWS_CALLBACK_CLOSED:                                      return "LWS_CALLBACK_CLOSED";
            case LWS_CALLBACK_SERVER_WRITEABLE:                            return "LWS_CALLBACK_SERVER_WRITEABLE";
            case LWS_CALLBACK_RECEIVE:                                     return "LWS_CALLBACK_RECEIVE";
            case LWS_CALLBACK_RECEIVE_PONG:                                return "LWS_CALLBACK_RECEIVE_PONG";
            case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:                     return "LWS_CALLBACK_WS_PEER_INITIATED_CLOSE";
            case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:                  return "LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION";
            case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:                      return "LWS_CALLBACK_CONFIRM_EXTENSION_OKAY";
            case LWS_CALLBACK_WS_SERVER_BIND_PROTOCOL:                     return "LWS_CALLBACK_WS_SERVER_BIND_PROTOCOL";
            case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL:                     return "LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL";
            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:                     return "LWS_CALLBACK_CLIENT_CONNECTION_ERROR";
            case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:                 return "LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH";
            case LWS_CALLBACK_CLIENT_ESTABLISHED:                          return "LWS_CALLBACK_CLIENT_ESTABLISHED";
            case LWS_CALLBACK_CLIENT_CLOSED:                               return "LWS_CALLBACK_CLIENT_CLOSED";
            case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:              return "LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER";
            case LWS_CALLBACK_CLIENT_RECEIVE:                              return "LWS_CALLBACK_CLIENT_RECEIVE";
            case LWS_CALLBACK_CLIENT_RECEIVE_PONG:                         return "LWS_CALLBACK_CLIENT_RECEIVE_PONG";
            case LWS_CALLBACK_CLIENT_WRITEABLE:                            return "LWS_CALLBACK_CLIENT_WRITEABLE";
            case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:          return "LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED";
            case LWS_CALLBACK_WS_EXT_DEFAULTS:                             return "LWS_CALLBACK_WS_EXT_DEFAULTS";
            case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:                   return "LWS_CALLBACK_FILTER_NETWORK_CONNECTION";
            case LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL:                     return "LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL";
            case LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL:                     return "LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL";
            case LWS_CALLBACK_GET_THREAD_ID:                               return "LWS_CALLBACK_GET_THREAD_ID";
            case LWS_CALLBACK_ADD_POLL_FD:                                 return "LWS_CALLBACK_ADD_POLL_FD";
            case LWS_CALLBACK_DEL_POLL_FD:                                 return "LWS_CALLBACK_DEL_POLL_FD";
            case LWS_CALLBACK_CHANGE_MODE_POLL_FD:                         return "LWS_CALLBACK_CHANGE_MODE_POLL_FD";
            case LWS_CALLBACK_LOCK_POLL:                                   return "LWS_CALLBACK_LOCK_POLL";
            case LWS_CALLBACK_UNLOCK_POLL:                                 return "LWS_CALLBACK_UNLOCK_POLL";
            case LWS_CALLBACK_CGI:                                         return "LWS_CALLBACK_CGI";
            case LWS_CALLBACK_CGI_TERMINATED:                              return "LWS_CALLBACK_CGI_TERMINATED";
            case LWS_CALLBACK_CGI_STDIN_DATA:                              return "LWS_CALLBACK_CGI_STDIN_DATA";
            case LWS_CALLBACK_CGI_STDIN_COMPLETED:                         return "LWS_CALLBACK_CGI_STDIN_COMPLETED";
            case LWS_CALLBACK_CGI_PROCESS_ATTACH:                          return "LWS_CALLBACK_CGI_PROCESS_ATTACH";
            case LWS_CALLBACK_SESSION_INFO:                                return "LWS_CALLBACK_SESSION_INFO";
            case LWS_CALLBACK_GS_EVENT:                                    return "LWS_CALLBACK_GS_EVENT";
            case LWS_CALLBACK_HTTP_PMO:                                    return "LWS_CALLBACK_HTTP_PMO";
            case LWS_CALLBACK_RAW_PROXY_CLI_RX:                            return "LWS_CALLBACK_RAW_PROXY_CLI_RX";
            case LWS_CALLBACK_RAW_PROXY_SRV_RX:                            return "LWS_CALLBACK_RAW_PROXY_SRV_RX";
            case LWS_CALLBACK_RAW_PROXY_CLI_CLOSE:                         return "LWS_CALLBACK_RAW_PROXY_CLI_CLOSE";
            case LWS_CALLBACK_RAW_PROXY_SRV_CLOSE:                         return "LWS_CALLBACK_RAW_PROXY_SRV_CLOSE";
            case LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE:                     return "LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE";
            case LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE:                     return "LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE";
            case LWS_CALLBACK_RAW_PROXY_CLI_ADOPT:                         return "LWS_CALLBACK_RAW_PROXY_CLI_ADOPT";
            case LWS_CALLBACK_RAW_PROXY_SRV_ADOPT:                         return "LWS_CALLBACK_RAW_PROXY_SRV_ADOPT";
            case LWS_CALLBACK_RAW_PROXY_CLI_BIND_PROTOCOL:                 return "LWS_CALLBACK_RAW_PROXY_CLI_BIND_PROTOCOL";
            case LWS_CALLBACK_RAW_PROXY_SRV_BIND_PROTOCOL:                 return "LWS_CALLBACK_RAW_PROXY_SRV_BIND_PROTOCOL";
            case LWS_CALLBACK_RAW_PROXY_CLI_DROP_PROTOCOL:                 return "LWS_CALLBACK_RAW_PROXY_CLI_DROP_PROTOCOL";
            case LWS_CALLBACK_RAW_PROXY_SRV_DROP_PROTOCOL:                 return "LWS_CALLBACK_RAW_PROXY_SRV_DROP_PROTOCOL";
            case LWS_CALLBACK_RAW_RX:                                      return "LWS_CALLBACK_RAW_RX";
            case LWS_CALLBACK_RAW_CLOSE:                                   return "LWS_CALLBACK_RAW_CLOSE";
            case LWS_CALLBACK_RAW_WRITEABLE:                               return "LWS_CALLBACK_RAW_WRITEABLE";
            case LWS_CALLBACK_RAW_ADOPT:                                   return "LWS_CALLBACK_RAW_ADOPT";
            case LWS_CALLBACK_RAW_CONNECTED:                               return "LWS_CALLBACK_RAW_CONNECTED";
            case LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL:                       return "LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL";
            case LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL:                       return "LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL";
            case LWS_CALLBACK_RAW_ADOPT_FILE:                              return "LWS_CALLBACK_RAW_ADOPT_FILE";
            case LWS_CALLBACK_RAW_RX_FILE:                                 return "LWS_CALLBACK_RAW_RX_FILE";
            case LWS_CALLBACK_RAW_WRITEABLE_FILE:                          return "LWS_CALLBACK_RAW_WRITEABLE_FILE";
            case LWS_CALLBACK_RAW_CLOSE_FILE:                              return "LWS_CALLBACK_RAW_CLOSE_FILE";
            case LWS_CALLBACK_RAW_FILE_BIND_PROTOCOL:                      return "LWS_CALLBACK_RAW_FILE_BIND_PROTOCOL";
            case LWS_CALLBACK_RAW_FILE_DROP_PROTOCOL:                      return "LWS_CALLBACK_RAW_FILE_DROP_PROTOCOL";
            case LWS_CALLBACK_TIMER:                                       return "LWS_CALLBACK_TIMER";
            case LWS_CALLBACK_EVENT_WAIT_CANCELLED:                        return "LWS_CALLBACK_EVENT_WAIT_CANCELLED";
            case LWS_CALLBACK_CHILD_CLOSING:                               return "LWS_CALLBACK_CHILD_CLOSING";
            case LWS_CALLBACK_VHOST_CERT_AGING:                            return "LWS_CALLBACK_VHOST_CERT_AGING";
            case LWS_CALLBACK_VHOST_CERT_UPDATE:                           return "LWS_CALLBACK_VHOST_CERT_UPDATE";
            case LWS_CALLBACK_MQTT_NEW_CLIENT_INSTANTIATED:                return "LWS_CALLBACK_MQTT_NEW_CLIENT_INSTANTIATED";
            case LWS_CALLBACK_MQTT_IDLE:                                   return "LWS_CALLBACK_MQTT_IDLE";
            case LWS_CALLBACK_MQTT_CLIENT_ESTABLISHED:                     return "LWS_CALLBACK_MQTT_CLIENT_ESTABLISHED";
            case LWS_CALLBACK_MQTT_SUBSCRIBED:                             return "LWS_CALLBACK_MQTT_SUBSCRIBED";
            case LWS_CALLBACK_MQTT_CLIENT_WRITEABLE:                       return "LWS_CALLBACK_MQTT_CLIENT_WRITEABLE";
            case LWS_CALLBACK_MQTT_CLIENT_RX:                              return "LWS_CALLBACK_MQTT_CLIENT_RX";
            case LWS_CALLBACK_MQTT_UNSUBSCRIBED:                           return "LWS_CALLBACK_MQTT_UNSUBSCRIBED";
            case LWS_CALLBACK_MQTT_DROP_PROTOCOL:                          return "LWS_CALLBACK_MQTT_DROP_PROTOCOL";
            case LWS_CALLBACK_MQTT_CLIENT_CLOSED:                          return "LWS_CALLBACK_MQTT_CLIENT_CLOSED";
            case LWS_CALLBACK_MQTT_ACK:                                    return "LWS_CALLBACK_MQTT_ACK";
            case LWS_CALLBACK_MQTT_RESEND:                                 return "LWS_CALLBACK_MQTT_RESEND";
            case LWS_CALLBACK_USER:                                        return "LWS_CALLBACK_USER";

        }

        return "????";
    }



} // ns

