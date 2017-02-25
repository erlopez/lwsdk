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
#include "HttpUtils.h"
#include "Strings.h"
#include "Exceptions.h"
#include <sstream>

// See docs:
// https://curl.se/libcurl/c/curl_easy_setopt.html
// https://curl.se/libcurl/c/libcurl-tutorial.html
#include <curl/curl.h>

using namespace std;

namespace lwsdk::HttpUtils
{
    enum RequestType { HTTP_GET, HTTP_POST, HTTP_HEAD };
    static bool isVerbose = false;

    void setVerbose( bool verboseEnabled )
    {
        isVerbose = verboseEnabled;
    }

    std::string urlEncode( const std::string &s )
    {
        char *tmp = curl_easy_escape( nullptr, s.c_str(), (int)s.length() );
        string res( tmp );
        curl_free( tmp );

        return res;
    }

    std::string urlDecode( const std::string &s )
    {
        int outlen = 0;
        char *tmp = curl_easy_unescape( nullptr, s.c_str(), (int)s.length(), &outlen );
        string res( tmp, outlen );
        curl_free( tmp );

        return res;
    }


    /**
     * Consume incoming header data
     * @param buffer  Contains data
     * @param size    Always one (char size)
     * @param nmemb   Size of the data
     * @param userp   User-data, pointer to incoming buffer used to store data
     * @return        Bytes consumed, must be equal to nmemb or curl ends transfer with error
     */
    static size_t curlHeaderCallback(void *buffer, size_t size, size_t nmemb, void *userp )
    {
        auto s = (stringstream*) userp;
        s->write( (char*)buffer, nmemb );  //  hint: size is always 1, nmemb is the size of the data

        return nmemb;
    }
    
    /**
     * Consume incoming response data
     * @param buffer  Contains data
     * @param size    Always one (char size)
     * @param nmemb   Size of the data
     * @param userp   User-data, pointer to incoming buffer used to store data
     * @return        Bytes consumed, must be equal to nmemb or curl ends transfer with error
     */
    static size_t curlWriteCallback(void *buffer, size_t size, size_t nmemb, void *userp )
    {
        auto res = (HttpResponse*) userp;
        res->text.append( (char*)buffer, nmemb );  //  hint: size is always 1, nmemb is the size of the data
        
        return nmemb;
    }


    /**
     * Issue a HTTP request and return the response
     * @param reqType             Request type: HEAD, GET, POST, etc.
     * @param url                 URL endpoint
     * @param isInsecure          Set to true if calling https on a self-signed site to skipt host CA
     *                            validation, otherwise set false
     * @param connectTimeoutSec   Time in seconds to abort connection is remote server cannot be reach,
     *                            set to 0 to use default (300 seconds)
     * @param urlEncodedData      URL-encoded HTTP name/value pair parameters to be posted, used only on POST
     * @param headers             Optional HTTP headers, given as name/value string pairs
     * @return                    Response data. The result is on the .text field; if an error occurs
     *                            .status would be different than 0 and .err would contains the error message.
     */
    static HttpResponse doRequest( RequestType reqType, const std::string& url, bool isInsecure, long connectTimeoutSec,
                                   const std::string& urlEncodedData, const HttpHeaders& headers )
    {
        HttpResponse response{};
        CURL *handle;
        CURLcode res;
        struct curl_slist *curlHeaders = nullptr;
        stringstream responseHeaders;

        handle = curl_easy_init();
        if ( handle )
        {
            curl_easy_setopt( handle, CURLOPT_URL, url.c_str() );
            curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, 1L );            // redirected, so we tell libcurl to follow redirection
            curl_easy_setopt( handle, CURLOPT_VERBOSE, isVerbose ? 1L : 0L );  // send debug info to stdout

            if ( connectTimeoutSec > 0 )
            {
                //curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);          // bail if transaction takes more than N seconds (include connection time)
                curl_easy_setopt( handle, CURLOPT_CONNECTTIMEOUT, connectTimeoutSec );  // bail if connection takes more than N seconds
            }

            if ( isInsecure )
            {
                curl_easy_setopt( handle, CURLOPT_SSL_VERIFYHOST, 0 );  // disable host verification
                curl_easy_setopt( handle, CURLOPT_SSL_VERIFYPEER, 0 );  // disable CA verification
            }

            // Set callback function to read http response
            curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, curlWriteCallback );
            curl_easy_setopt( handle, CURLOPT_WRITEDATA, &response );  // set userp* argument to callback

            // Set callback function to read response headers
            curl_easy_setopt( handle, CURLOPT_HEADERFUNCTION, curlHeaderCallback );
            curl_easy_setopt( handle, CURLOPT_HEADERDATA, &responseHeaders );  // set userp* argument to callback

            // Add custom headers
            if ( !headers.empty() )
            {
                for ( const auto& h : headers )
                    curlHeaders = curl_slist_append( curlHeaders, (h.first + ": " + h.second).c_str() );

                if ( curlHeaders != nullptr )
                    curl_easy_setopt( handle, CURLOPT_HTTPHEADER, curlHeaders );

            }

            // Add post data, if any
            // Hint: Setting the CURLOPT_POSTFIELDS option tells libcurl to perform POST instead of GET
            if ( reqType == HTTP_POST )
                curl_easy_setopt( handle, CURLOPT_POSTFIELDS, urlEncodedData.c_str() );

            
            // For HEAD, set custom request
            if ( reqType == HTTP_HEAD )
            {
                curl_easy_setopt( handle, CURLOPT_NOBODY, 1 ); // Do not wait for body response
                curl_easy_setopt( handle, CURLOPT_CUSTOMREQUEST, "HEAD" );
            }


            // Perform the request, res will get the return code
            res = curl_easy_perform( handle );

            // Check for errors
            if ( res != CURLE_OK )
            {
                response.errmsg = curl_easy_strerror(res);
                response.err = res;
                //loge( "curl_easy_perform() failed: %s\n", response.err.c_str() );
            }

            // Response status
            res = curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &response.code );
            if ( res != CURLE_OK )
                response.code = -1;


            // Parse and collect response headers, if any
            if ( responseHeaders.tellp() > 0 )
            {
                auto lines = Strings::split( responseHeaders.str(), "[\\r\\n]+" );
                for ( const auto &line : lines )
                {
                    if ( Strings::isEmpty(line) )
                        continue;

                    auto pos = line.find( ':' );
                    if ( pos == string::npos )
                        continue;

                    response.headers.emplace( Strings::trim( line.substr(0, pos) ),
                                              Strings::trim( line.substr(pos+1)  ) );

                    //printf( "res headers --> '%s'\n",  line.c_str() );
                }
            }

            // Free the header list
            if ( curlHeaders != nullptr )
                curl_slist_free_all( curlHeaders );

            // close handle connection
            curl_easy_cleanup( handle );

        }

        return response;
    }


    HttpResponse get( const std::string & url, bool isInsecure, long connectTimeoutSec, const HttpHeaders& headers )
    {
        return doRequest( HTTP_GET, url, isInsecure, connectTimeoutSec, "", headers );
    }

    HttpResponse head( const std::string & url, bool isInsecure, long connectTimeoutSec, const HttpHeaders& headers )
    {
        return doRequest( HTTP_HEAD, url, isInsecure, connectTimeoutSec, "", headers );
    }


    HttpResponse post( const std::string & url, bool isInsecure, long connectTimeoutSec, const HttpParams& params, const HttpHeaders& headers )
    {
        stringstream data;

        // urlencode post parameters
        if ( !params.empty() )
        {
            for ( const auto& p : params )
            {
                if ( data.tellp() > 0 )
                    data << '&';

                data << urlEncode( p.first ) << "=" << urlEncode( p.second );
            }
        }

        return doRequest( HTTP_POST, url, isInsecure, connectTimeoutSec, data.str(), headers );
    }


    HttpResponse post( const std::string & url, bool isInsecure, long connectTimeoutSec, const std::string& urlEncodedData, const HttpHeaders& headers )
    {
        return doRequest( HTTP_POST, url, isInsecure, connectTimeoutSec, urlEncodedData, headers );
    }


} //ns
