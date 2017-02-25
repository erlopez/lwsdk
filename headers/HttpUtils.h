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
#ifndef HTTPUTILS_H
#define HTTPUTILS_H

#include <string>
#include <map>
#include "Utils.h"

namespace lwsdk::HttpUtils
{
    using HttpParams = std::map<std::string, std::string>;
    using HttpHeaders = std::map<std::string, std::string, lwsdk::Utils::CaseInsensitiveComparator>;

    static const HttpHeaders NO_HEADERS{};
    static const HttpParams NO_PARAMS{};

    struct HttpResponse {
        long code;             // HTTP response code, e.g. 200 OK
        std::string text;      // Response text
        int err;               // libcurl error number, 0 if no errors
        std::string errmsg;    // libcurl error message, "" blank if no errors
        HttpHeaders headers;   // server response headers, empty if none
    };


    /**
     * Enable/disable CURL verbosity. When enabled, extra debug information
     * is sent to the stderr.
     * @param verboseEnabled true enables verbosity, false sets normal operation.
     */
    void setVerbose( bool verboseEnabled );

    /**
     * Encode string value for sending over GET or POST requests
     * @param s String to be encoded
     * @return URL-encoded value
     */
    std::string urlEncode( const std::string &s );

    /**
     * Decode string value from URL forms parameters
     * @param s String to be decoded
     * @return Decoded value
     */
    std::string urlDecode( const std::string &s );


    /**
     * Issue a GET request and returns response.
     * @param url                 URL endpoint
     * @param isInsecure          Set to true if calling https on a self-signed site to skipt host CA
     *                            validation, otherwise set false
     * @param connectTimeoutSec   Time in seconds to abort connection is remote server cannot be reach,
     *                            set to 0 to use default (300 seconds)
     * @param headers             Optional HTTP headers, given as name/value string pairs
     * @return                    Response data
     */
    HttpResponse get( const std::string & url, bool isInsecure= false, long connectTimeoutSec= 0,
                      const HttpHeaders& headers= NO_HEADERS );

    /**
     * Issue a HEAD request and returns response.
     * @param url                 URL endpoint
     * @param isInsecure          Set to true if calling https on a self-signed site to skipt host CA
     *                            validation, otherwise set false
     * @param connectTimeoutSec   Time in seconds to abort connection is remote server cannot be reach,
     *                            set to 0 to use default (300 seconds)
     * @param headers             Optional HTTP headers, given as name/value string pairs
     * @return                    Response data
     */
    HttpResponse head( const std::string & url, bool isInsecure= false, long connectTimeoutSec= 0,
                       const HttpHeaders& headers= NO_HEADERS );


    /**
     * Issue a POST request and return the response
     * @param url                 URL endpoint
     * @param isInsecure          Set to true if calling https on a self-signed site to skipt host CA
     *                            validation, otherwise set false
     * @param connectTimeoutSec   Time in seconds to abort connection is remote server cannot be reach,
     *                            set to 0 to use default (300 seconds)
     * @param params              HTTP name/value pair parameters; these are url-encoded automatically
     * @param headers             Optional HTTP headers, given as name/value string pairs
     * @return                    Response data
     */
    HttpResponse post( const std::string & url, bool isInsecure= false, long connectTimeoutSec= 0,
                       const HttpParams& params= NO_PARAMS, const HttpHeaders& headers= NO_HEADERS );


    /**
     * Issue a POST request and return the response
     * @param url                 URL endpoint
     * @param isInsecure          Set to true if calling https on a self-signed site to skipt host CA
     *                            validation, otherwise set false
     * @param connectTimeoutSec   Time in seconds to abort connection is remote server cannot be reach,
     *                            set to 0 to use default (300 seconds)
     * @param urlEncodedData      URL-encoded HTTP name/value pair parameters to be posted
     * @param headers             Optional HTTP headers, given as name/value string pairs
     * @return                    Response data
     */
    HttpResponse post( const std::string & url, bool isInsecure, long connectTimeoutSec= 0,
                       const std::string& urlEncodedData= "", const HttpHeaders& headers= NO_HEADERS );


} // ns


#endif /*HTTPUTILS_H */
