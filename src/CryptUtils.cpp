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
#include "CryptUtils.h"
#include "Strings.h"

#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#define LOGGER_ENABLED 0   // turn off verbose logging
#include "Logger.h"
#include "Utils.h"


// See docs https://www.openssl.org/docs/man1.1.1/man3/BIO_write.html
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

using namespace std;


namespace lwsdk::CryptUtils
{
    static recursive_mutex cryptMutex;

    // CRC32 lookup table to speed up crc32 calculations
    static const uint32_t CRC32_POLYNOMIAL = 0xEDB88320;  // magic polynomial
    static uint32_t crc32Cache[256];                      // Lookup Table

    // Variables used for scrambling operations
    static const int AES256_CBC_KEY_SIZE = 32;
    static const int AES256_CBC_BLOCK_SIZE = 16;
    static string sha512Passphrase;                      // holds SHA512 of the user-defined passphrase

    // Text value used to represent empty strings
    static const string JUNK_VALUE = "2e90CaUDa0eL2==";

    // Fwd
    static void hashPassphrase( const std::string &s );

    /**
     * Clean up on exit
     */
    static void cleanup()
    {
        // Removes all digests and ciphers
        EVP_cleanup();

        // if you omit the next, a small leak may be left when you make use of the BIO
        // (low level API) for e.g. base64 transformations
        CRYPTO_cleanup_all_ex_data();

        // Remove error strings
        ERR_free_strings();

        logY( "CryptUtils cleanup" );
    }

    /**
     * Init static contect first time around
     */
    static void init()
    {
        static bool inited = false;
        if ( inited )
            return;

        // Populate CRC32 cache
        uint32_t crc;
        for ( uint32_t i = 0; i < 256; i++ )
        {
            crc = i;
            for ( int j = 0; j < 8; j++ )
                crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLYNOMIAL : 0);

            crc32Cache[i] = crc;
        }

        // Load the human readable error strings for libcrypto
        ERR_load_crypto_strings();

        // Load all digest and cipher algorithms
        OpenSSL_add_all_algorithms();

        // Load config file, and other important initialisation
        //OPENSSL_config( NULL );

        // Use library's default passphrase key
        hashPassphrase( "" );

        logY( "CryptUtils inited" );
        inited = true;
        atexit( cleanup );
    }


    /**
     * Helper function to compute digest algorithms.
     * @param algo  Algorithm name: MD5, SHA256, etc.
     * @param data  Data to be processed
     * @param len   Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    static ResultString doDigest( const std::string &algo, const uint8_t *data, int len )
    {
        EVP_MD_CTX *mdctx;
        const EVP_MD *md;
        unsigned char buf[EVP_MAX_MD_SIZE];
        unsigned int mdlen;
        string retval;

        md = EVP_get_digestbyname( algo.c_str());
        if ( md == nullptr )
            return { false, algo + " is not a valid digest; EVP_get_digestbyname() failed." };

        mdctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex( mdctx, md, nullptr );
        EVP_DigestUpdate( mdctx, data, len );
        EVP_DigestFinal_ex( mdctx, buf, &mdlen );
        EVP_MD_CTX_free( mdctx );

        retval.append((const char *) buf, mdlen );

        return { true, retval };
    }

    /**
     * Update the internal hash of the user-defined phrase
     */
    static void hashPassphrase( const std::string &s )
    {
        string phrase = !s.empty() ? s :
                       "It is recommended to call configure() and change this value as "
                       "it is used as salt to uniquely scramble strings in this library.";

        auto result = doDigest( "SHA512", (uint8_t*)phrase.data(), (int)phrase.length() );

        if ( !result.isValid() )
            throw RuntimeException( string( __PRETTY_FUNCTION__ ) + " - error hashing user passphrase: " + result.value() );

        sha512Passphrase = result.value();

        logC("sha512Passphrase: %s ", toHexString( result.value() ).c_str() );
    }


    void configure( const std::string &s )
    {
        lock_guard lock( cryptMutex );
        init();

        hashPassphrase( s );
    }


    ResultString md5( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();

        return doDigest( "MD5", data, len );
    }

    ResultString md5( const std::string &data )
    {
        return md5( (uint8_t *)data.data(), (int)data.length() );
    }


    ResultString sha1( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();

        return doDigest( "SHA1", data, len );
    }

    ResultString sha1( const std::string &data )
    {
        return sha1( (uint8_t *)data.data(), (int)data.length() );
    }


    ResultString sha256( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();

        return doDigest( "SHA256", data, len );
    }

    ResultString sha256( const std::string &data )
    {
        return sha256( (uint8_t *)data.data(), (int)data.length() );
    }


    ResultString sha512( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();

        return doDigest( "SHA512", data, len );
    }

    ResultString sha512( const std::string &data )
    {
        return sha512( (uint8_t *)data.data(), (int)data.length() );
    }


    uint32_t crc32( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();

        uint32_t crc = 0xFFFFFFFF;

        for ( int i = 0; i < len; i++ )
            crc = (crc >> 8) ^ crc32Cache[(crc ^ data[i]) & 0xFF];

        return ~crc;
    }

    uint32_t crc32( const std::string& data )
    {
        return crc32( (uint8_t *)data.data(), (int)data.length() );
    }


    std::string toHexString( const std::string &s, bool isUpperCase )
    {
        ostringstream oss;
        oss << (isUpperCase ? std::uppercase : std::nouppercase) << std::setfill( '0' ) << std::hex;

        for ( const auto ch: s )
            oss << std::setw(2) << (ch & 0xff); // hint: setw() is not sticky, needs to be re-issued with every value

        return oss.str();
    }


    std::string toHexString( uint64_t val, int width, bool isUpperCase )
    {
        ostringstream oss;
        oss << (isUpperCase ? std::uppercase : std::nouppercase)
            << std::setfill( '0' ) << std::setw( width ) << std::hex << val;

        return oss.str();
    }


    ResultString encodeBase64( const std::string &data )
    {
        return encodeBase64((uint8_t *) data.c_str(), (int) data.length());
    }


    ResultString encodeBase64( const uint8_t *data, int len )
    {
        lock_guard lock( cryptMutex );
        init();


        // Note: Base64 encodes each set of three bytes into four bytes.
        //       the final size is: ceil( input_size / 3.0 ) * 4
        const int BUF_SIZE = 512;                 // process data in manageable chunks at the time, make sure is a factor of 4
        const int CHUNK_SIZE = BUF_SIZE * 3 / 4;  // max amount of data that can be encoded per write() call that will fit in buffer w/o overflowing
        char buf[BUF_SIZE];

        BIO *b64, *bio;
        ostringstream oss;

        // Tip: BIO_f_base64() is a filter BIO that encodes any data written through it
        // and decodes any data read through it.
        b64 = BIO_new( BIO_f_base64());
        BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL ); // put everything in one line

        // Attach buffer for BIO filter to write encoded data
        bio = BIO_new( BIO_s_mem());
        BIO_push( b64, bio );

        // for debugging internal buffer
        //BUF_MEM *bufferPtr;
        //BIO_get_mem_ptr(bio, &bufferPtr);
        //printf( "--> %p  %ld\n",  bufferPtr->data, (long)bufferPtr->length );


        logB( "total len is %d bytes", len );
        int total = 0;
        while ( total < len )
        {
            // Encode next chunk
            int chunk = std::min( len - total, CHUNK_SIZE );

            int nWritten = BIO_write( b64, &data[total], chunk );
            logC( "wrote %d bytes", nWritten );
            if ( nWritten <= 0 )
            {
                total = -1;
                break;
            }
            total += nWritten;

            // reached the end of the data, then add any required padding = or ==
            if ( total == len )
                BIO_flush( b64 );

            int nRead = BIO_read( bio, buf, sizeof( buf ));
            if ( nRead > 0 )
                oss.write( buf, nRead );
        }

        BIO_free_all( b64 );

        if ( total < 0 )
            return { false, "encodeBase64() encoding failed; BIO_write() error." };
        else
            return { true, oss.str() };
    }


    ResultString decodeBase64( const std::string &data )
    {
        lock_guard lock( cryptMutex );
        init();

        BIO *b64, *bio;
        ostringstream oss;
        char buf[512];
        int len = (int) data.length();

        // Tip: BIO_f_base64() is a filter BIO that encodes any data written through it
        // and decodes any data read through it.
        b64 = BIO_new( BIO_f_base64());
        BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL ); // input might have LF, ignore them

        // Attach buffer containing encoded data
        bio = BIO_new_mem_buf( data.data(), len );  // makes a read-only buffer
        BIO_push( b64, bio );

        logB( "total len is %d bytes", len );

        int nRead;
        do
        {
            nRead = BIO_read( b64, buf, sizeof( buf ));
            logC( "read %d bytes", nRead );

            if ( nRead > 0 )
                oss.write( buf, nRead );

        } while ( nRead > 0 );

        BIO_free_all( b64 );


        if ( nRead < 0 )
            return { false, "base64 decoding failed; malformed encoding" };
        else
            return { true, oss.str() };
    }


    ResultString scramble( const std::string &s )
    {
        return scramble((uint8_t *) s.data(), (int) s.length());
    }


    ResultString scramble( const uint8_t *data, int len )
    {         
        lock_guard lock( cryptMutex );
        init();

        const uint8_t* src = data;

        // Replace empty data by junk
        if ( len == 0 )
        {
            src = (uint8_t*)JUNK_VALUE.data();
            len = (int)JUNK_VALUE.length();
        }

        // Scramble data using SHA512 of user-defined passphrase as a binary mask
        int hashSize = (int)sha512Passphrase.length();
        string buf(len, 0);

        for ( int i=0; i < len; i++)
            buf[i] = (char) (src[i] ^ sha512Passphrase[i % hashSize]);

        // Convert to printable text
        auto result = encodeBase64( buf );
        if ( !result.isValid())
            return {false, "scramble() error - " + result.value()};

        buf = Strings::replaceAll( result.value(), "y", "yK" );
        buf = Strings::replaceAll( buf, "\\+", "yp" );
        buf = Strings::replaceAll( buf, "==", "yj" );
        buf = Strings::replaceAll( buf, "=", "yq" );
        buf = Strings::replaceAll( buf, "/", "yS" );

        // Add crc32 suffix
        return { true, buf + 'g' + toHexString( crc32( buf ), 8 ) };
    }


    ResultString unscramble( const uint8_t *data, int len )
    {
        return  unscramble( string( (char*)data, len) );
    }


    ResultString unscramble( const std::string &s )
    {
        lock_guard lock( cryptMutex );
        init();

        // Has valid signature?
        if ( !isScrambled(s) )
            return { false, "Unscramble failed, bad checksum or string was tampered width." };

        // strip off checksum suffix
        string buf(s, 0, s.length() - 9 );

        // Unscramble base64 to get bytes
        buf = Strings::replaceAll( buf, "yp", "+" );
        buf = Strings::replaceAll( buf, "yj", "==" );
        buf = Strings::replaceAll( buf, "yq", "=" );
        buf = Strings::replaceAll( buf, "yS", "/" );
        buf = Strings::replaceAll( buf, "yK", "y" );
        auto result = decodeBase64( buf );

        if ( !result.isValid())
            return { false, "Unscramble decodeBase64() failed - " + result.value() };

        buf = result.value();

        // Unmask the data using sha512Passphrase
        int hashSize = (int)sha512Passphrase.length();

        for ( int i = 0; i < (int)buf.length(); i++ )
            buf[i] = (char) (buf[i] ^ sha512Passphrase[i % hashSize]);

        // If junk is found, clear buffer
        if ( buf == JUNK_VALUE )
            buf.clear();

        return { true, buf };
    }


    bool isScrambled( const std::string &s )
    {
        int pos = s.length() - 9; // checksum start pos

        if ( pos < 0 || !Strings::matches( s.substr(pos), "g[0-9a-fA-F]+$" ) )
            return false;

        string crc = s.substr( pos + 1 );
        string buf = s.substr( 0, pos );

        return crc == toHexString( crc32(buf) );

    }



    /**
     * Encrypts a data buffer
     *
     * @param data   Plain text
     * @param len    Length of the given data
     * @param salt   Optional, user-defined data to spice the initial vector used by the
     *               cipher; is used, the salt for the decryption must be the same used
     *               during encryption. Note it is ok for the salt to have non-printable
     *               characters.
     * @param usingScrambling true causes the returned cipher text be in scrambled format,
     *               otherwise cipher text is returned raw.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    static ResultString encrypt( const uint8_t *data, int len, const std::string &salt, bool usingScrambling )
    {
        lock_guard lock( cryptMutex );
        init();

        // The passphraseSha512 is a 64 byte long sha512 hash of user defined phrase
        // the first 32 bytes are used as 'key', the 16-bytes that follows used as 'iv'
        string iv( sha512Passphrase, AES256_CBC_KEY_SIZE, AES256_CBC_BLOCK_SIZE );  // iv = initial vector

        // If salt is given, further infuse the iv
        if ( salt.length() > 0 )
        {
            auto result = md5(salt);
            if ( !result.isValid() )
                return { false, "Failed to hash salt - "s + result.value() };

            // XOR the hash on top of the iv
            for ( int i = 0; i < AES256_CBC_BLOCK_SIZE; i++ )
                iv[i] = char( iv[i] ^ result.value()[i % result.value().length()] );
        }

        // Create and initialize the context
        EVP_CIPHER_CTX *ctx;
        char errorText[256];

        if ( !(ctx = EVP_CIPHER_CTX_new()) )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            return { false, "Failed to create EVP cipher context - "s + errorText };
        }

        // Initialise the encryption operation
        // We are using 256 bit AES CBC cipher;
        //    key is 256 bit (32 bytes)
        //    iv is 128 bit (16 byte); same as cipher block size
        if ( EVP_EncryptInit_ex( ctx, EVP_aes_256_cbc(), nullptr,
                                 (uint8_t*)sha512Passphrase.data(), (uint8_t*)iv.data() ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to init encrypt cipher - "s + errorText };
        }

        // Allocate space for the output cipher text.
        // Cipher text can expand up to BLOCK_SIZE of the original plain text
        auto cipherText = std::make_unique<uint8_t[]>(len + AES256_CBC_BLOCK_SIZE);
        int totalCount;
        int nCount;

        // Provide the message to be encrypted, and obtain the encrypted output.
        if ( EVP_EncryptUpdate( ctx, cipherText.get(), &nCount, data, len ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to update encrypt cipher text- "s + errorText };
        }

        totalCount = nCount;

        // Finalize the encryption. Further ciphertext bytes may be written at this stage.
        if ( EVP_EncryptFinal_ex( ctx, cipherText.get() + nCount, &nCount ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to finalize encrypt cipher text- "s + errorText };
        }

        totalCount += nCount;

        // Clean up
        EVP_CIPHER_CTX_free( ctx );


        // Scramble result, adds tampering protection that can be verified at
        // the time of decryption
        if ( usingScrambling )
        {
            auto result = scramble( string((char *) cipherText.get(), totalCount ));

            if ( !result.isValid())
                return { false, "Failed to scramble result - "s + result.value() };


            return { true, result.value() };
        }
        else
        {
            return { true, string((char *) cipherText.get(), totalCount ) };
        }

    }


    ResultString encrypt( const std::string &s, const std::string &salt )
    {
        return encrypt( (uint8_t*)s.data(), (int)s.length(), salt, true );
    }

    ResultString encrypt( const uint8_t *data, int len, const std::string &salt )
    {
        return encrypt( data, len, salt, true );
    }


    /**
     * Decrypts a data buffer
     *
     * @param data   If usingScrambling=false, this is the raw cipher text; if usingScrambling=true
     *               the cipher text is deducted from unscrambling this text
     * @param len    length of the given data
     * @param salt   Optional, user-defined data to spice the initial vector used by the
     *               cipher; the salt used for the decryption must be the same used
     *               during encryption. Note it is ok for the salt to have non-printable
     *               characters.
     * @param usingScrambling true indicates the given data is in scrambled format, otherwise
     *               data is raw cipher data.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    static ResultString decrypt( const uint8_t *data, int len, const std::string &salt, bool usingScrambling )
    {
        lock_guard lock( cryptMutex );
        init();

        string cipherText;

        // Unscramble result, and check for any tampering
        if ( usingScrambling )
        {
            auto result = unscramble( data, len );
            if ( !result.isValid())
                return { false, "Failed to unscramble data - "s + result.value() };

            cipherText = result.value();
        }
        else
        {
            cipherText.append((char*)data, len );
        }

        // The passphraseSha512 is a 64 byte long sha512 hash of user defined phrase
        // the first 32 bytes are used as 'key', the 16-bytes that follows used as 'iv'
        string iv( sha512Passphrase, AES256_CBC_KEY_SIZE, AES256_CBC_BLOCK_SIZE );  // iv = initial vector

        // If salt is given, further infuse the iv
        if ( salt.length() > 0 )
        {
            auto result = md5(salt);
            if ( !result.isValid() )
                return { false, "Failed to hash salt - "s + result.value() };

            // XOR the hash on top of the iv
            for ( int i = 0; i < AES256_CBC_BLOCK_SIZE; i++ )
                iv[i] = char( iv[i] ^ result.value()[i % result.value().length()] );
        }


        // Create and initialize the context
        EVP_CIPHER_CTX *ctx;
        char errorText[256];

        if ( !(ctx = EVP_CIPHER_CTX_new()) )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            return { false, "Failed to create EVP cipher context - "s + errorText };
        }

        // Initialise the encryption operation
        // We are using 256 bit AES CBC cipher;
        //    key is 256 bit (32 bytes)
        //    iv is 128 bit (16 byte); same as cipher block size
        if ( EVP_DecryptInit_ex( ctx, EVP_aes_256_cbc(), nullptr,
                                 (uint8_t*)sha512Passphrase.data(), (uint8_t*)iv.data() ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to init decrypt cipher - "s + errorText };
        }

        // Allocate space for the output plain text.
        // Plain text contracts as much as to BLOCK_SIZE from the given cipher text
        auto plainText = std::make_unique<uint8_t[]>( cipherText.length() );
        int totalCount;
        int nCount;

        // Provide the message to be decrypted, and obtain the plain text output.
        if ( EVP_DecryptUpdate( ctx, plainText.get(), &nCount,
                                (uint8_t*)cipherText.data(), (int)cipherText.length() ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to update decrypt cipher text- "s + errorText };
        }

        totalCount = nCount;

        // Finalize the decryption. Remaining plain text bytes may be written at this stage.
        if ( EVP_DecryptFinal_ex( ctx, plainText.get() + nCount, &nCount ) != 1 )
        {
            ERR_error_string_n( ERR_get_error(), errorText, sizeof(errorText) );
            EVP_CIPHER_CTX_free( ctx );
            return { false, "Failed to finalize decrypt cipher text- "s + errorText };
        }

        totalCount += nCount;

        // Clean up
        EVP_CIPHER_CTX_free( ctx );

        return { true, string((char*)plainText.get(), totalCount) };

    }


    ResultString decrypt( const std::string &s, const std::string &salt )
    {
        return decrypt( (uint8_t*)s.data(), (int)s.length(), salt, true );
    }


    ResultString decrypt( const uint8_t *data, int len, const std::string &salt )
    {
        return decrypt( data, len, salt, true );
    }



    ResultString hash( const std::string &s )
    {
        // Generate salt
        string salt = toHexString( crc32( std::to_string(Utils::currentTimeMillis())) );

        // Encrypt text
        auto cipherText = encrypt( (uint8_t*)s.data(), (int)s.length(), salt, false );

        if ( !cipherText.isValid() )
            return { false, "Failed to hash text - "s + cipherText.value() };


        // Create hash string, salt followed "y", and then the cipher text
        string hash = salt + 'y' + cipherText.value();

        // Scramble and return the hash
        auto result = scramble( hash );

        if ( !result.isValid())
            return { false, "Failed to hash text - "s + result.value() };
        else
            return { true, result.value() };

    }


    ResultString dehash( const std::string &s )
    {
        // Retrieve hash string from scrambled envelope
        auto result = unscramble( s );
        if ( !result.isValid() )
            return { false, "Failed to dehash text - "s + result.value() };

        // Check the salt prefix is there
        if ( result.value().length() < 9 || !Strings::matches( result.value().substr(0,9), "^[0-9a-fA-F]{8}y" ) )
            return { false, "Failed to dehash text - malformed hash text, bad salt prefix." };

        string salt = result.value().substr( 0, 8 );    // before 'y' character
        string cipherText = result.value().substr( 9 ); // after 'y' character

        // Decrypt remaining text using salt
        result = decrypt( (uint8_t *)cipherText.data(), (int)cipherText.length(), salt, false );
        if ( !result.isValid() )
            return { false, "Failed to dehash text - "s + result.value() };

        return { true, result.value() };

    }



} //ns