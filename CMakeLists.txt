cmake_minimum_required(VERSION 3.1)
project(lwsdk)

#set(CMAKE_VERBOSE_MAKEFILE on)

# Set libs and include sources
find_package(Libwebsockets REQUIRED)           # libwebsockets-dev
find_package(CURL REQUIRED)                    # libcurl4-openssl-dev
find_package(LIBSSH REQUIRED)                  # libssh-dev
find_package(OpenSSL REQUIRED)                 # libopenssl-dev

# Specify C++ std
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# Compiler settings
add_compile_options( -g -Wall -Wno-unused-result -Wno-unused )

# Set variables for other projects
set(LWSDK_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/headers" CACHE INTERNAL "")
set(LWSDK_LIB_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
set(LWSDK_LIBRARIES "lwsdk" CACHE INTERNAL "")


# Sources
set(LWSDK_SOURCES
        src/Utils.cpp
        src/Strings.cpp
        src/CryptUtils.cpp
        src/Files.cpp
        src/Config.cpp
        src/Terminal.cpp
        src/SerialPort.cpp
        src/NetlinkUEvent.cpp
        src/NetlinkRoute.cpp
        src/SSHConnection.cpp
        src/UDPSocketServer.cpp
        src/UDPSocketClient.cpp
        src/HttpUtils.cpp
        src/Webserver.cpp
)

set(LWSDK_HEADERS
        headers/lwsdk.h
        headers/Logger.h
        headers/Exceptions.h
        headers/ConcurrentQueue.tpp
        headers/ConcurrentQueue.h
        headers/Result.h
        headers/Utils.h
        headers/Strings.h
        headers/CryptUtils.h
        headers/Files.h
        headers/Config.h
        headers/Terminal.h
        headers/SerialPort.h
        headers/NetlinkUEvent.h
        headers/NetlinkRoute.h
        headers/UDPSocketServer.h
        headers/UDPSocketClient.h
        headers/SSHConnection.h
        headers/HttpUtils.h
        headers/Webserver.h
)

# Build library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR} )

add_library(lwsdk STATIC ${LWSDK_HEADERS} ${LWSDK_SOURCES} )
target_include_directories(lwsdk PUBLIC headers ${LIBWEBSOCKETS_INCLUDE_DIRS}
        {LIBSSH_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})
target_link_libraries(lwsdk ${LIBWEBSOCKETS_LIBRARIES}  ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES} ssh )

 