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
#ifndef SSHCONNECTION_H
#define SSHCONNECTION_H


#include <fcntl.h>
#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <condition_variable>

// Docs:  https://api.libssh.org/stable/libssh_tutorial.html
#include <libssh/libssh.h>


namespace lwsdk
{
    static const char * SSH_PUBKEY = "@@SSH_PUBKEY";

    // Store information from remote downloaded file
    struct SSHRemoteFileInfo {
        size_t size;   // remote file size
        int mode;      // remote file permissions
    };

    class SSHConnection;

    /**
     * User callback to receive SSH connection open/close events.
     * Use ssh.isOpen() to know whether or not the SSH connection is open.
     * @param ssh         The ssh connection instance
     *
     */
    typedef std::function<void( SSHConnection &ssh )>
            SSHStatusCallback_t;

    /**
     * User callback to receive lines of text send over SSH connection
     * @param ssh       The ssh connection instance
     * @param line      The received text line
     */
    typedef std::function<void( SSHConnection &ssh, const std::string &line )>
            SSHLineCallback_t;

    /**
     * User callback to receive characters of received over SSH connection
     * @param ssh       The ssh connection instance
     * @param data      Characters received
     * @param len       Number of characters received
     */
    typedef std::function<void( SSHConnection &ssh, const char *data, uint32_t len )>
            SSHDataCallback_t;

    
    class SSHConnection
    {
        ssh_session session{nullptr};  // ssh connection
        ssh_channel channel{nullptr};  // main channel for shell pty

        std::string info;

        bool verbose{false};
        long timeout{0};
        void *userdata{nullptr};

        int lastError{0};
        std::string lastErrorMsg{};

        SSHLineCallback_t   lineCallback{nullptr};
        SSHDataCallback_t   dataCallback{nullptr};
        SSHStatusCallback_t statusCallback{nullptr};

        std::atomic_bool  keepWorking{false};
        std::atomic_bool  isConnected{false};
        std::thread      *sshReaderThread{nullptr};
        
        std::condition_variable idleCondVar;

        void readerThread();
        int  setError( int error, const std::string& errorMsg );
        void disconnect();

        void scpWrite( std::istream& in, const std::string& destDir,
                       const std::string& destFile,
                       size_t destFileSize, int destFileMode );

        void scpRead( std::ostream& out, const std::string& remoteFile,
                      SSHRemoteFileInfo *fileInfo );

    public:
        SSHConnection();

        virtual ~SSHConnection();

        /**
         * Set additional connection options
         * @param verbose  Print underlying connection logs to stderr
         * @param timeout  Set a timeout for the connection in seconds
         */
        void setOptions( bool verbose, long timeout );

        /**
         * Set/clear user data that can be accessed in callbacks
         * @param userdata  User-defined data
         */
        void setUserData( void * userdata );

        /**
         * Retrieve any user data attached to this connection instance
         * @return User-defined data
         */
        void * getUserData();

        /**
         * Retrieve any connection information
         * @return User-defined data
         */
        std::string getInfo();

        /**
         * Test if the last operation ended up with errors.
         */
        bool hasErrors();

        /**
         * Returns the last operation error code (if any); 0 if
         * no errors occurred.
         */
        int getLastError();

        /**
         * Returns the last operation error information (if any); blank if
         * no errors occurred.
         */
        std::string getLastErrorMsg();

        /**
         * Clear any outstanding errors.
         */
        void clearErrors();

        /**
         * Set user-defined callback to receive SSH port open/close events.
         * @param statusCallback  Pointer to user-defined function. Set to null
         *                        to remove any previously set callback.
         */
        void setStatusCallback( const SSHStatusCallback_t& statusCallback );

        /**
         * Set user-defined callback to receive raw SSH port data.
         * @param dataCallback  Pointer to user-defined function. Set to null
         *                      to remove any previously set callback.
         */
        void setDataCallback( const SSHDataCallback_t& dataCallback );

        /**
         * Set user-defined callback to receive line-delimited SSH port text.
         * @param dataCallback  Pointer to user-defined function. Set to null
         *                      to remove any previously set callback.
         */
        void setLineCallback( const SSHLineCallback_t& lineCallback );

        /**
         * Open SSH connection to remote host using username and password.
         * After returning, see getLastError() and getLastErrorMsg() for
         * errors.
         *
         * @param host         Remote host IP address
         * @param port         Remote host SSH port
         * @param user         Remote username
         * @param passwd       Remote user password. Set to SSH_PUBKEY to try
         *                     private keys in current user ~/.ssh for
         *                     authentication. 
         */
        void open( const std::string &host, uint32_t port,
                   const std::string &user, const std::string &passwd = SSH_PUBKEY );


        /**
         * Closes the SSH port. Does nothing if the connection is not open.
         */
        void close();

        /**
         * Test if the SSH port is open for communications.
         */
        bool isOpen();

        /**
         * Writes text to the SSH port. After returning,
         * see getLastError() and getLastErrorMsg() for errors.
         *
         * @param text Text to send.
         * @return The number of bytes sent. Returns -1 on error.
         */
        int write( const std::string &text );

        /**
         * Writes raw data to the SSH port.   After returning,
         * see getLastError() and getLastErrorMsg() for errors.
         *
         * @param data Data to send out.
         * @return The number of bytes sent. Returns -1 on error.
         */
        int write( const uint8_t *data, int len );

        /**
         * Execute a shell command remotely and returns its output.
         * The result contains any stderr followed by any stdout.
         * After returning,  see getLastError() and getLastErrorMsg()
         * for errors.
         *
         * @param cmd  Shell command to run
         * @return     Output of the command: stderr + stdout
         */
        std::string exec( const std::string &cmd );


        /**
         * Copy local file to remote file.  After returning,
         * see getLastError() and getLastErrorMsg() for errors.
         *
         * @param srcFile   Path to local file, absolute or relative
         * @param destFile  Full absolute path to remote file; it cannot
         *                  be just remote directory.
         * @param mode      File permissions, chmod-like octal value like 0664
         *                  or OR'ed combination of these flags:
         *                     User:  S_IRUSR, S_IWUSR, S_IXUSR = S_IRWXU,
         *                     Group: S_IRGRP, S_IWGRP, S_IXGRP = S_IRWXG,
         *                     Other: S_IROTH, S_IWOTH, S_IXOTH = S_IRWXO,
         */
        void uploadFile( const std::string& srcFile, const std::string& destFile,
                         int mode = S_IRWXU | S_IRWXG | S_IXOTH | S_IROTH);


        /**
         * Create remote file with the given string  data contents.  After
         * returning, see getLastError() and getLastErrorMsg() for errors.
         *
         * @param data      String value saved to remoted file
         * @param destFile  Full absolute path to remote file; it cannot
         *                  be just remote directory.
         * @param mode      File permissions, chmod-like octal value like 0664
         *                  or OR'ed combination of these flags:
         *                     User:  S_IRUSR, S_IWUSR, S_IXUSR = S_IRWXU,
         *                     Group: S_IRGRP, S_IWGRP, S_IXGRP = S_IRWXG,
         *                     Other: S_IROTH, S_IWOTH, S_IXOTH = S_IRWXO,
         */
        void uploadFileFromString( const std::string& data, const std::string& destFile,
                                   int mode = S_IRWXU | S_IRWXG | S_IXOTH | S_IROTH);


        /**
         * Download a server remote file to a local file.  After
         * returning, see getLastError() and getLastErrorMsg() for errors.
         *
         * @param remoteFile  Absolute path to remote file being downloaded
         * @param localFile   Pathname to a local file to be create or ovewritten.
         * @param fileInfo    If given, it is populate with the file size and
         *                    remote file permissions.
         */
        void downloadFile( const std::string& remoteFile, const std::string& localFile,
                           SSHRemoteFileInfo *fileInfo = nullptr );

        /**
         * Returns the contents of a server remote file as a string.  After
         * returning, see getLastError() and getLastErrorMsg() for errors.
         *
         * @param remoteFile  Absolute path to remote file being downloaded
         * @param localFile   Pathname to a local file to be create or ovewritten.
         * @param fileInfo    If given, it is populate with the file size and
         *                    remote file permissions.
         *
         * @return The remote file contents. This value should be used only if
         *         no errors were reported after the operation.
         */
        std::string downloadFileAsString( const std::string& remoteFile,
                                          SSHRemoteFileInfo *fileInfo = nullptr );

        /**
         * Retrieve the remote server fingerprint. Before opening a formal connection
         * you may first probe the remote server and get its fingerprint. As a
         * security measure you can save the fingerprint the first time around
         * in a trusted list so you can use to validate the server's identity in
         * subsequent connections.
         *
         * After returning, see getLastError() and getLastErrorMsg() for errors.
         *
         * @param host       Remote server IP address.
         * @param port       Remote SSH server port
         * @param useSha256  True returns sha256 hash, false returns sha1 hash.
         * @return           Hex string of the fingerprint
         */
        std::string getServerFingerprint( const std::string& host, uint32_t port, bool useSha256 = true);


        /**
         * Retrieve the remote server welcome banner. This can be done without
         * logging in. Most servers do not offer banners, some embedded devices offer
         * manufacturer information or serial numbers.
         *
         * After returning, see getLastError() and getLastErrorMsg() for errors.
         *
         * @param host       Remote server IP address.
         * @param port       Remote SSH server port
         * @return           Banner text, if any, otherwise blank "".
         */
        std::string getServerBanner( const std::string& host, uint32_t port );
    };


}
#endif //SSHCONNECTION_H
