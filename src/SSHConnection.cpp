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
#include <sstream>
#include <mutex>
#include <thread>
#include <optional>


#include <cstring>
#include <string>
#include <fstream>


#include "SSHConnection.h"
#include "Files.h"

//#define DUMP_ENABLED       // turn on mem dumps
#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"


using namespace std;
namespace fs = std::filesystem;

namespace lwsdk
{

    SSHConnection::SSHConnection()
    {
        // Start reader thread
        keepWorking = true;
        isConnected = false;
        sshReaderThread = new thread( &SSHConnection::readerThread, this );
    }

    SSHConnection::~SSHConnection()
    {
        // End reader thread
        if ( sshReaderThread != nullptr )
        {
            keepWorking = false;       // signal reader thread to exit
            idleCondVar.notify_one();  // wake thread if sleeping
            sshReaderThread->join();   // Wait thread exit
            delete sshReaderThread;    // clean up
            sshReaderThread = nullptr;
        }

        // Close port
        close();
    }

    void SSHConnection::setOptions( bool verbose, long timeout )
    {
        this->verbose = verbose;
        this->timeout = timeout;
    }

    void SSHConnection::setUserData( void *userdata )
    {
        this->userdata = userdata;
    }

    void *SSHConnection::getUserData()
    {
        return userdata;
    }

    std::string SSHConnection::getInfo()
    {
        return info;
    }

    bool SSHConnection::hasErrors()
    {
        return lastError != 0;
    }

    int SSHConnection::getLastError()
    {
        return lastError;
    }

    std::string SSHConnection::getLastErrorMsg()
    {
        return lastErrorMsg;
    }

    int SSHConnection::setError( int error, const std::string& errorMsg )
    {
        lastError = error;
        lastErrorMsg = errorMsg;
        return error;
    }

    void SSHConnection::clearErrors()
    {
        lastError = 0;
        lastErrorMsg.clear();
    }

    void SSHConnection::setStatusCallback( const SSHStatusCallback_t& statusCallback )
    {
        this->statusCallback = statusCallback;
    }

    void SSHConnection::setDataCallback( const SSHDataCallback_t& dataCallback )
    {
        this->dataCallback = dataCallback;
    }


    void SSHConnection::setLineCallback( const SSHLineCallback_t& lineCallback )
    {
        this->lineCallback = lineCallback;
    }


    bool SSHConnection::isOpen()
    {
        return isConnected;
    }


    int SSHConnection::write( const std::string& text )
    {
        return write( (uint8_t*)text.data(), (int) text.length() );
    }


    int SSHConnection::write( const uint8_t *data, int len  )
    {
        int count = 0;
        int bytesWritten;

        clearErrors();

        #ifdef DUMP_ENABLED
        printf( FG14 );
        Utils::memdump( data, len );
        printf( NOC );
        #endif

        while ( count < len && isConnected )
        {

            // send to ssh terminal
            bytesWritten = ssh_channel_write( channel, &data[count], len - count );

            if ( bytesWritten == SSH_ERROR )
            {
                setError( -1, "Error while writing to " + info );
                logw( "%s", lastErrorMsg.c_str() );
                return -1;
            }

            count += bytesWritten;
            //logi( "Wrote to %s (%d of %d) bytes", info.c_str(), count, len );

        } // while

        return count;
    }


    std::string SSHConnection::exec( const std::string &cmd )
    {
        int rc;
        ssh_channel cmdChannel;

        clearErrors();

        // Open EXEC channel
        cmdChannel = ssh_channel_new( session );
        if ( channel == nullptr )
        {
            setError( -1, "Error creating command channel for: " + cmd );
            return "";
        }

        rc = ssh_channel_open_session( cmdChannel );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error opening command channel for: " + cmd + " -- " + ssh_get_error( session ) );
            ssh_channel_free( cmdChannel );
            return "";
        }

        // Run command
        rc = ssh_channel_request_exec( cmdChannel, cmd.c_str() );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error executing command for: " + cmd + " -- " + ssh_get_error( session ) );
            ssh_channel_send_eof( cmdChannel );
            ssh_channel_close( cmdChannel );
            ssh_channel_free( cmdChannel );
            return "";
        }

        // Read output, read stderr first, followed by stdout
        stringstream ss;
        char buffer[256];
        int nbytes;

        for ( int fd=1; fd>=0; fd--) // hint: fd=1 stderr, fd=0 stdout
        {
            do
            {
                nbytes = ssh_channel_read( cmdChannel, buffer, sizeof( buffer ), fd );
                if ( nbytes < 0 )
                {
                    setError( rc, "Error reading command output for: " + cmd + " -- " + ssh_get_error( session ));
                    ssh_channel_send_eof( cmdChannel );
                    ssh_channel_close( cmdChannel );
                    ssh_channel_free( cmdChannel );
                    return "";
                }

                ss.write( buffer, nbytes );

            } while ( nbytes > 0 );
        }

        ssh_channel_send_eof( cmdChannel );
        ssh_channel_close( cmdChannel );
        ssh_channel_free( cmdChannel );

        logB( "exec ('%s'):\n%s", cmd.c_str(), ss.str().c_str());

        return ss.str();
    }


    void SSHConnection::scpWrite( std::istream& in, const std::string& destDir, const std::string& destFile,
                                  size_t destFileSize, int destFileMode )
    {
        int rc;
        ssh_scp scp;
        string pathname = Files::mkpath(destDir, destFile);

        // set remote folder to create file in
        scp = ssh_scp_new( session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, destDir.c_str() );
        if ( scp == nullptr )
        {
            setError( -1, "Error setting scp location at: " + destDir + " -- " + ssh_get_error( session ) );
            return;
        }

        rc = ssh_scp_init( scp );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error initializing scp location at: " + destDir + " -- " + ssh_get_error( session ) );
            ssh_scp_free( scp );
            return;
        }

        // open remote file for writing
        rc = ssh_scp_push_file ( scp, destFile.c_str(), destFileSize, destFileMode );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error opening remote file for writing: " + pathname + " -- " + ssh_get_error( session ) );
            ssh_scp_free( scp );
            return;
        }

        // copy file data over
        try
        {
            const int BUF_SZ = 1024;
            auto buf = make_unique<char[]>(BUF_SZ); // freed automatically at the end of the scope

            while ( !in.eof() )
            {
                in.read( buf.get(), BUF_SZ );
                rc = ssh_scp_write( scp, buf.get(), in.gcount() );

                if ( rc != SSH_OK )
                {
                    setError( rc, "Error writing to remote file: " + pathname + " -- " + ssh_get_error( session ) );
                    break;
                }
            }

        }
        catch ( const exception &e )
        {
            setError( rc, "IO Error while writing to: " + pathname + " -- " + e.what() );
        }

        ssh_scp_close( scp );
        ssh_scp_free( scp );
 
    }


    void SSHConnection::uploadFileFromString( const std::string& data, const std::string& destFile, int mode )
    {
        clearErrors();

        try
        {
            fs::path remotePathname( destFile );
            istringstream iss(data, std::ios_base::in |  std::ios_base::binary );

            scpWrite( iss, remotePathname.parent_path().string(), remotePathname.filename().string(), data.length(), mode );

        }
        catch ( const exception &e )
        {
            setError( -1, "IO Error while creating file from string -- "s + e.what() );
        }

    }


    void SSHConnection::uploadFile( const std::string& srcFile, const std::string& destFile, int mode )
    {
        clearErrors();

        // make sure local file is there
        if ( !Files::isFile( srcFile ))
        {
            setError( -1, "File not found: " + srcFile );
            return;
        }

        // open file for reading and pass input stream to scp write function
        try
        {
            size_t fsize = Files::getFileSize( srcFile );
            fs::path remotePathname( destFile );

            fstream fis( srcFile, ios_base::in | ios_base::binary );  // fstream closes automatically at the end of the scope

            scpWrite( fis, remotePathname.parent_path().string(), remotePathname.filename().string(), fsize, mode );

        }
        catch ( const exception &e )
        {
            setError( -1, "IO Error while copying: " + srcFile + " -- " + e.what() );
        }

    }


    void SSHConnection::scpRead( std::ostream& out, const std::string& remoteFile, SSHRemoteFileInfo *fileInfo )
    {
        int rc;
        ssh_scp scp;

        clearErrors();

        // create scp session
        scp = ssh_scp_new( session, SSH_SCP_READ, remoteFile.c_str() );
        if ( scp == nullptr )
        {
            setError( -1, "Error creating scp session to download: " + remoteFile + " -- " + ssh_get_error( session ) );
            return;
        }

        // open scp channel
        rc = ssh_scp_init( scp );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error creating scp channel to download: " + remoteFile + " -- " + ssh_get_error( session ) );
            ssh_scp_free( scp );
            return;
        }

        // request next file
        // If you opened the SCP session in recursive mode, the remote end will be telling you when to change directory.
        // In that case, when ssh_scp_pull_request() answers SSH_SCP_REQUEST_NEWDIRECTORY, you should make that
        // local directory (if it does not exist yet) and enter it. When ssh_scp_pull_request() answers
        // SSH_SCP_REQUEST_ENDDIRECTORY, you should leave the current directory.
        //
        // SSH_SCP_REQUEST_WARNING are errors like "SCP: Warning: scp: /tmp/helloworld.txt: No such file or directory"


        rc = ssh_scp_pull_request( scp );
        if ( rc != SSH_SCP_REQUEST_NEWFILE )
        {
            ssh_scp_deny_request( scp, "Expecting a file!");

            setError( rc, "Error remote download does not seems to be a file: " + remoteFile + " -- " + ssh_get_error( session ) );
            ssh_scp_close( scp );
            ssh_scp_free( scp );
            return;
        }
 
        // get file info
        size_t size = ssh_scp_request_get_size( scp );
        int mode = ssh_scp_request_get_permissions( scp );

        if ( fileInfo != nullptr )
        {
            fileInfo->size = size;
            fileInfo->mode = mode;
        }

        //string filename = ssh_scp_request_get_filename( scp );
        //logY( "Receiving file %s, size %ld, permissions 0%o\n", filename.c_str(), size, mode );


        // open file for reading
        char buffer[256];

        rc = ssh_scp_accept_request( scp );
        if ( rc != SSH_OK )
        {
            setError( rc, "Error accepting pull request download: " + remoteFile + " -- " + ssh_get_error( session ) );
            ssh_scp_close( scp );
            ssh_scp_free( scp );
            return;
        }

        // Read file
        do {
            int count = ssh_scp_read( scp, buffer, sizeof( buffer ));
            if ( count == SSH_ERROR )
            {
                setError( -1, "Error reading file download: " + remoteFile + " -- " + ssh_get_error( session ) );
                ssh_scp_close( scp );
                ssh_scp_free( scp );
                return;
            }

            out.write( buffer, count );

            //logY( "  count %d", (int) out.tellp());

        } while ( out.tellp() < (long) size );


        //logY( "Done\n" );

        // Get next file, should be EOF mark
        rc = ssh_scp_pull_request( scp );
        if ( rc != SSH_SCP_REQUEST_EOF )
            setError( -1, "Error Expecting only one file: " + remoteFile + " but server is sending more that one." );


        // clean up
        ssh_scp_close( scp ); // Close the scp channel
        ssh_scp_free( scp );  // Close the scp session

    }


    std::string SSHConnection::downloadFileAsString( const std::string& remoteFile, SSHRemoteFileInfo *fileInfo  )
    {
        clearErrors();

        ostringstream oss;

        // open file for writing and pass output stream to scp read function
        try
        {
            scpRead( oss, remoteFile, fileInfo );

        }
        catch ( const exception &e )
        {
            setError( -1, "IO Error while downloading " + remoteFile + " -- " + e.what() );
        }

        return oss.str();
    }

    
    void SSHConnection::downloadFile( const std::string& remoteFile, const std::string& localFile,
                                      SSHRemoteFileInfo *fileInfo  )
    {
        clearErrors();

        // make sure local path is not a directory is there
        if ( Files::isDir( localFile ))
        {
            setError( -1, "localFile cannot point to a directory: " + localFile );
            return;
        }

        // open file for writing and pass output stream to scp read function
        try
        {
            fstream fos( localFile, ios_base::out | ios_base::binary );  // fstream closes automatically at the end of the scope
            scpRead( fos, remoteFile, fileInfo );

        }
        catch ( const exception &e )
        {
            setError( -1, "IO Error while downloading " + remoteFile + " to " + localFile + " -- " + e.what() );
        }

    }


    std::string SSHConnection::getServerBanner( const std::string& host, uint32_t port )
    {
        int rc;
        string endpoint = host + ":" + to_string(port);

        clearErrors();

        // Configure SSH instance
        ssh_session probeSession = ssh_new();
        if ( probeSession == nullptr )
        {
            setError( -1, "Error creating new ssh instance for " + endpoint );
            return "";
        }

        int verbosity = this->verbose ? SSH_LOG_PROTOCOL : SSH_LOG_NOLOG;
        ssh_options_set( probeSession, SSH_OPTIONS_LOG_VERBOSITY, &verbosity );
        ssh_options_set( probeSession, SSH_OPTIONS_HOST, host.c_str() );
        ssh_options_set( probeSession, SSH_OPTIONS_PORT, &port );

        if ( this->timeout > 0 )
            ssh_options_set( probeSession, SSH_OPTIONS_TIMEOUT, &this->timeout  );


        // Connect
        rc = ssh_connect( probeSession );
        if ( rc != SSH_OK )
        {
            setError( rc, "Connect failed " + endpoint + " - " + ssh_get_error( probeSession ) );
            ssh_disconnect( probeSession );
            ssh_free( probeSession );
            return "";
        }

        // Perform dummy login to pull ssh banner
        rc = ssh_userauth_none( probeSession, nullptr );
        if ( rc == SSH_AUTH_ERROR )
        {
            setError( -1, "Error authenticating with none: " + endpoint + " -- " + ssh_get_error( probeSession ) );
            ssh_disconnect( probeSession );
            ssh_free( probeSession );
            return "";
        }

        // Read banner text
        string banner;

        char *tmp = ssh_get_issue_banner( probeSession );
        if ( tmp )
        {
            banner = tmp;
            free( tmp );
        }

        ssh_disconnect( probeSession );
        ssh_free( probeSession );

        return banner;

    }

    
    std::string SSHConnection::getServerFingerprint( const std::string& host, uint32_t port, bool useSha256 )
    {
        int rc;
        size_t hashLen;
        uint8_t *hash = nullptr;
        ssh_key serverPubkey = nullptr;
        string endpoint = host + ":" + to_string(port);

        clearErrors();

        // Configure SSH instance
        ssh_session probeSession = ssh_new();
        if ( probeSession == nullptr )
        {
            setError( -1, "Error creating new ssh instance for " + endpoint );
            return "";
        }

        int verbosity = this->verbose ? SSH_LOG_PROTOCOL : SSH_LOG_NOLOG;
        ssh_options_set( probeSession, SSH_OPTIONS_LOG_VERBOSITY, &verbosity );
        ssh_options_set( probeSession, SSH_OPTIONS_HOST, host.c_str() );
        ssh_options_set( probeSession, SSH_OPTIONS_PORT, &port );

        if ( this->timeout > 0 )
            ssh_options_set( probeSession, SSH_OPTIONS_TIMEOUT, &this->timeout  );


        // Connect
        rc = ssh_connect( probeSession );
        if ( rc != SSH_OK )
        {
            setError( rc, "Connect failed " + endpoint + " - " + ssh_get_error( probeSession ) );
            ssh_disconnect( probeSession );
            ssh_free( probeSession );
            return "";
        }

        // Read server public key
        rc = ssh_get_server_publickey( probeSession, &serverPubkey );
        if ( rc != SSH_OK )
        {
            setError( -1, "Error reading server public key: " + endpoint + " -- " + ssh_get_error( probeSession ) );
            ssh_disconnect( probeSession );
            ssh_free( probeSession );
            return "";
        }

        // Extract public key hash
        stringstream fingerprint;

        rc = ssh_get_publickey_hash( serverPubkey, useSha256 ? SSH_PUBLICKEY_HASH_SHA256 : SSH_PUBLICKEY_HASH_SHA1,
                                     &hash, &hashLen );

        ssh_key_free( serverPubkey );

        if ( rc == SSH_OK )
        {
            char *hexstr = ssh_get_hexa( hash, hashLen );
            fprintf( stderr, "Public key hash: %s\n", hexstr );
            ssh_string_free_char( hexstr );

            fingerprint << std::nouppercase << std::setfill( '0' ) << std::setw( 2 ) << std::hex;  // format stream for 2-digit hex

            for ( size_t i = 0; i < hashLen; i++ )
                fingerprint << (uint32_t) hash[i];

            ssh_clean_pubkey_hash( &hash );
        }
        else
        {
            setError( -1, "Error reading file download: " + endpoint + " -- " + ssh_get_error( probeSession ) );
        }

        ssh_disconnect( probeSession );
        ssh_free( probeSession );

        return fingerprint.str();

    }


    void SSHConnection::close()
    {
        clearErrors();
        disconnect();
        info.clear();
    }


    void SSHConnection::disconnect()
    {
        // Close and free resources
        if ( channel != nullptr )
        {
            ssh_channel_close( channel );
            ssh_channel_send_eof( channel );
            ssh_channel_free( channel );
            channel = nullptr;
        }

        if ( session != nullptr )
        {
            ssh_disconnect( session );
            ssh_free( session );
            session = nullptr;
        }
 
        info.clear();

        // Put read thread into standby, notify user of disconnect event
        if ( isConnected )
        {
            isConnected = false;
            logi( "Disconnected SSH %s", info.c_str() );

            if ( statusCallback )
                statusCallback( *this );
        }

    }


    void SSHConnection::open( const std::string &host, uint32_t port, const std::string &user, const std::string &passwd )
    {
        disconnect();
        clearErrors();

        int rc;
        int verbosity = this->verbose ? SSH_LOG_PROTOCOL : SSH_LOG_NOLOG;

        this->info = user + "@" + host + ":" + std::to_string(port);


        // Create SSH instance
        session = ssh_new();
        if ( session == nullptr )
        {
            setError( -1, "Error creating new ssh instance for " + info );
            return;
        }

        // Configure connection
        // see https://api.libssh.org/stable/group__libssh__session.html#ga7a801b85800baa3f4e16f5b47db0a73d

        ssh_options_set( session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity );
        ssh_options_set( session, SSH_OPTIONS_HOST, host.c_str() );
        ssh_options_set( session, SSH_OPTIONS_PORT, &port );
        ssh_options_set( session, SSH_OPTIONS_USER, user.c_str() );

        if ( this->timeout > 0 )
            ssh_options_set( session, SSH_OPTIONS_TIMEOUT, &this->timeout  );


        // Connect
        rc = ssh_connect( session );
        if ( rc != SSH_OK )
        {
            setError( rc, "Connect failed " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }
 
        // Authenticate with pub-key or password
        if ( passwd == SSH_PUBKEY )
        {
            // Pubkey, uses current user keys in ~/.ssh folder
            rc = ssh_userauth_publickey_auto( session, nullptr, nullptr );
            if ( rc == SSH_AUTH_ERROR )
            {
                setError( rc, "Public key auth failed " + info + " - " + ssh_get_error( session ));
                disconnect();
                return;
            }
        }
        else
        {
            // password
            rc = ssh_userauth_password( session, nullptr, passwd.c_str());
            if ( rc != SSH_AUTH_SUCCESS )
            {
                setError( rc, "Password auth failed " + info + " - " + ssh_get_error( session ));
                disconnect();
                return;
            }
        }


        channel = ssh_channel_new( session );
        if ( channel == nullptr )
        {
            setError( rc, "Cannot create channel " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }

        rc = ssh_channel_open_session( channel );
        if ( rc != SSH_OK )
        {
            setError( rc, "Cannot open channel " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }

        rc = ssh_channel_request_pty( channel );
        if ( rc != SSH_OK )
        {
            setError( rc, "Cannot create pty " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }

        rc = ssh_channel_change_pty_size( channel, 100, 24 );
        if ( rc != SSH_OK )
        {
            setError( rc, "Cannot set pty size " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }

        rc = ssh_channel_request_shell( channel );
        if ( rc != SSH_OK )
        {
            setError( rc, "Cannot create shell " + info + " - " + ssh_get_error( session ) );
            disconnect();
            return;
        }

        // Wake reader thread
        isConnected = true;
        idleCondVar.notify_one();

        logi( "Connected to %s", info.c_str() );

        // Notify user callback if any
        if ( statusCallback )
            statusCallback( *this );
    }


    void SSHConnection::readerThread()
    {
        bool lastConnectionState = true;
        char buffer[256];
        int nbytes;
        string line;


        logi( "SSH Reader thread started" );

        //{
        //    std::lock_guard<std::mutex> lock(mtx);
        //}

        while ( keepWorking )
        {

            if ( lastConnectionState != isConnected )
            {
                lastConnectionState = isConnected;
                logi( "SSH Reader thread is %s", isConnected ? "ACTIVE" : "STANDBY" );
            }

            // If not connected, do short sleeps while standing by for connection getting open
            if ( !isConnected )
            {
                mutex mtx;
                unique_lock lock( mtx );
                idleCondVar.wait_for( lock, 10s, [this]() { return !this->keepWorking || this->isConnected; } );
                continue;
            }

            #ifdef DUMP_ENABLED
            printf( "o" );
            fflush( stdout );
            #endif


            nbytes = ssh_channel_read_nonblocking( channel, buffer, sizeof( buffer ), 0 );
            if ( nbytes < 0 )
            {
                // Remote host disconnected
                close();
                logw( "SSH connection terminated" );

                continue;
            }
            else if ( nbytes > 0 )
            {
                // call user defined data callback -- if any
                if ( dataCallback )
                {
                    try
                    {
                        dataCallback( *this, buffer, nbytes );
                    }
                    catch ( const std::exception& e )
                    {
                        loge( "User's dataCallback() finished with errors: %s", e.what() );
                    }
                }

                #ifdef DUMP_ENABLED
                Utils::memdump( buffer, nbytes );
                #endif

                // call user defined line callback -- if any
                if ( lineCallback )
                {

                    line.append( buffer, nbytes );
                    string s;

                    while ( !(s = Strings::findMatch(line,"[^\r\n]*(\r\n|\r|\n)")).empty() )
                    {      
                        try
                        {
                            line.erase(0, s.length() );
                            s = Strings::replaceAll(s, "[\r\n]+$", "");
                            lineCallback( *this, s );
                        }
                        catch ( const std::exception& e )
                        {
                            loge( "User's dataCallback() finished with errors: %s", e.what() );
                        }
                    } //while
                }

            }
            else
            {
                // nbytes = 0, no incoming data
                std::this_thread::sleep_for( 50ms );
            }


            
        } // while keepworking


        logi( "SSH Reader thread ended" );

    }


} // ns

