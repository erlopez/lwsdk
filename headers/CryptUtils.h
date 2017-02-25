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
#ifndef CRYPT_UTILS_H
#define CRYPT_UTILS_H

#include <string>
#include "Result.h"


/**
 *  Utility functions for mangling text.
 *
 *  For functions returning ResultString types, if isValid() is true,
 *  value() is the result from the string operation, otherwise value()
 *  is an error message.  For example:
 *
 *         auto result = CryptUtils::someFunction();
 *
 *         if ( result.isValid() )
 *            doSomething( result.value() );
 *         else
 *            printf( "Error: %s\n", result.value().c_str() )
 *
 *  All functions in this library are thread-safe.
 */
namespace lwsdk::CryptUtils
{
    /**
     * Computes the crc32 of the given data.
     * @param data  Data to be evaluated
     * @param len   Length of the data
     * @return crc32 result
     */
    uint32_t crc32( const uint8_t *data, int len );

    /**
     * Computes the crc32 of the given data.
     * @param data  Data to be evaluated
     * @return crc32 result
     */
    uint32_t crc32( const std::string& data );

    /**
     * Return string as hex encoded.
     * @param s            String to encode.
     * @param isUpperCase  Specifies to whether or not use capital letters
     * @return   Hex encoded representation string
     */
    std::string toHexString( const std::string &s, bool isUpperCase = false );

    /**
     * Return number as hex string.
     * @param val          Number to encode.
     * @param width        Number of digits in the string, padded with 0's to the left
     * @param isUpperCase  Specifies to whether or not use capital letters
     * @return   Hex representation of the number
     */
    std::string toHexString( uint64_t val, int width = 8, bool isUpperCase = false );

    /**
     * Encode the given string data as base 64
     * @param data Data to be encoded
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString encodeBase64( const std::string& data );

    /**
     * Encode the given data as base 64
     * @param data Data to be encoded
     * @param len  Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString encodeBase64( const uint8_t *data, int len );

    /**
     * Decode the given base64 string back to the original.
     *
     * @param data Data to be decoded
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString decodeBase64( const std::string& data  );

    /**
     * Computes the MD5 sum of the given data.
     * @param data  Data to be evaluated
     * @param len   Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString md5( const uint8_t *data, int len );
    ResultString md5( const std::string& data );


    /**
     * Computes the MD5 sum of the given data.
     * @param data  Data to be evaluated
     * @param len   Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString sha1( const uint8_t *data, int len );
    ResultString sha1( const std::string& data );


    /**
     * Computes the sha256 sum of the given data.
     * @param data  Data to be evaluated
     * @param len   Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString sha256( const uint8_t *data, int len );
    ResultString sha256( const std::string& data );


    /**
     * Computes the sha512 sum of the given data.
     * @param data  Data to be evaluated
     * @param len   Length of the data
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString sha512( const uint8_t *data, int len );
    ResultString sha512( const std::string& data );


    /**
     * Sets the passphrase used for subsequent encryption, hashing, and scrambling operations.
     * This value is global to the application. This method should be invoked when your application
     * starts, before any hashing and scrambling operations are performed.
     *
     * If configure() is not called, the library uses its default passphrase, but since this
     * is publicly available from the source code, it is recommended you set your own passphrase
     * (and keep it safe) if you plan to encrypt important stuff that lives in cold storage.
     *
     * @param s   A passphrase used as salt for subsequent encryption, hashing, and
     *            scrambling operations. Note it is ok for the passphrase to have non-printable
     *            characters.
     */
     void configure( const std::string& s );


    /**
     * This function is primarily used to scramble text so that it can be sent over public mediums,
     * like for example, hidden fields in a HTML form or CGI URL parameters.
     *
     * Scrambling is typically used to pass data along in URLs or HTML hidden form fields. Suppose
     * you pass the `shoppingCartId` along the URL as a parameter to another page.
     * Passing it as a plain number increases the risk of someone manually changing the URL
     * and potentially jumping into someone's else shopping cart contents. By scrambling the
     * parameter as something like "eTkxZTBjNzAwye882bc96", now the user
     * has no idea of how to alter this value. And if he does, he will corrupt the
     * checksum in the envelope enclosing the original value. Thus when the server receives
     * the parameter and validate it with the isScrambled() method, the program can take the
     * appropriate action if the test fails.
     *
     * The encoding does two things:
     *   - Convert all special and control characters into a steady alphanumeric character string.
     *   - Add a checksum tag to test if the character string has changed since it was first scrambled.
     *
     * Scrambling converts a text containing special characters such as quotes, CRLF, tabs
     * spaces, or other non-printable characters into a safe alphanumeric string that can be passed
     * along without breaking the a HTML or XML markup document, or a protocol that carries the data.
     *
     * In addition to character escaping, scrambling encloses the data in a tamper-proof envelope
     * that allows the receiver of the data to check if the data has been tampered with during transit.
     * To check if a given scrambled string is still valid before it is passed to the decoding method,
     * see isScrambled().
     *
     * Scrambling mangles data using the user-defined passphrase set via configure().  Without
     * the passphrase, trying to decipher any given scrambled text is theoretically impossible. Both
     * parties, the scrambler and the un-scrambler applications, need the same passphrase to communicate.
     *
     * @param s     Text to be scrambled.
     *
     * @param data  Data to be scrambled
     * @param len   Length of the data
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString scramble( const std::string& s );
    ResultString scramble( const uint8_t *data, int len );

    /**
     * Unscrambles a scrambled string. Prior to invoking this method, a string can be tested for
     * validity by passing the scrambled text to isScrambled(). isScrambled() returns true if the
     * given scrambled text is valid and can be unscrambled, otherwise false indicates the scrambled
     * text is corrupted or has been tampered with since it failed the checksum test.
     *
     * @param s     Scrambled string. This is a value generated by scramble() or encrypt().
     *              isScrambled() can be used to test if a scrambled string is valid.
     *
     * @param data  Data to be scrambled
     * @param len   Length of the data
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString unscramble( const std::string& s );
    ResultString unscramble( const uint8_t *data, int len  );


    /**
     * Tests the checksum in a given scrambled text. This method is used to validate a
     * scrambled string has not been manually tampered with after it was created.
     *
     * @param s  Scrambled text to be tested.
     * @return True if the given string is a valid scrambled text, false otherwise
     */
    bool isScrambled( const std::string& s );


    /**
     * Encrypts a string using the current user-defined passphrase.
     *
     * @param s     Text to be encrypted.
     *
     * @param data  Data to be encrypted
     * @param len   Length of the data
     *
     * @param salt  Optional, user-defined data to spice the initial vector used by the
     *              cipher; the salt used for the decryption must be the same used
     *              during encryption. Note it is ok for the salt to have non-printable
     *              characters.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString encrypt( const std::string &s, const std::string &salt = ""  );
    ResultString encrypt( const uint8_t *data, int len, const std::string &salt = "" );


    /**
     * Decrypts a string using the current user-defined passphrase.  
     *
     * @param s     Encrypted text. Before invoking this function, you can verify the checksum
     *              of the encrypted text to see if it has been tampered. Passing a encrypted
     *              text to isScrambled() returns true if the encrypted text is valid
     *              for decryption. If isScrambled() returns false it means the encrypted
     *              text has been altered and will fails the checksum validation.
     *
     * @param data  Data to be decrypted
     * @param len   Length of the data
     *
     * @param salt  Optional, user-defined data to spice the initial vector used by the
     *              cipher; the salt used for the decryption must be the same used
     *              during encryption. Note it is ok for the salt to have non-printable
     *              characters.
     *
     * @return The unencrypted text.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString decrypt( const std::string &s, const std::string &salt = "" );
    ResultString decrypt( const uint8_t *data, int len, const std::string &salt = "" );


    /**
     * Encrypts the given string using the current user-defined passphrase and
     * a time-variant salt.
     *
     * This function encrypts text using the current epoch time as added salt, thus
     * every millisecond that passes and you encrypt, let's say the word "Hello", the
     * result is different because the salt changes with time. Note the used salt is
     * embedded in the result so that dehash() can retrieved it later to recover the
     * original text.
     *
     * The underlying encryption in this method is also based on the user-defined
     * passphrase value set via configure(). Note that although the salt
     * is present inside the the payload, without the passphrase, the text
     * cannot be decrypted.
     *
     * This method is useful for generating activation codes, license numbers,
     * and random-looking text patterns used for other purposes.
     *
     * @param s   Text to be encrypted. Since the salt is time-variant, the same text
     *            values will produce different results.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString hash( const std::string &s );


    /**
     * Decrypts a hashed string. This method extracts the time-variant salt embedded inside a
     * hashed text and decrypts back the string to its original form.
     *
     * @param s  Hashed text. This is a value returned by the hash() method.
     *
     * @return A ResultString containing the result of the operation.
     *         If isValid() is true, value() has the result from
     *         the operation, otherwise value() is an error message.
     */
    ResultString dehash( const std::string &s );
    
}


#endif //CRYPT_UTILS_H
