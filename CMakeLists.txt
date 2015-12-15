cmake_minimum_required(VERSION 3.1)
project(lwsdk)

#set(CMAKE_VERBOSE_MAKEFILE on)

# Set libs and include sources
find_package(Libwebsockets REQUIRED)

# Specify C++ std
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# Compiler settings
add_compile_options( -g -Wall -Wno-unused-result -Wno-unused )

# Sources
set(LWSDK_SOURCES
        src/Utils.cpp
        src/Strings.cpp
        src/Files.cpp
        src/Config.cpp
        src/Terminal.cpp
        src/SerialPort.cpp
        src/NetlinkUEvent.cpp
        src/Webserver.cpp
)

set(LWSDK_HEADERS
        headers/lwsdk.h
        headers/Logger.h
        headers/Exceptions.h
        headers/ConcurrentQueue.tpp
        headers/ConcurrentQueue.h
        headers/Utils.h
        headers/Strings.h
        headers/Files.h
        headers/Config.h
        headers/Terminal.h
        headers/SerialPort.h
        headers/NetlinkUEvent.h
        headers/Webserver.h
)

# Build library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR} )

add_library(lwsdk STATIC ${LWSDK_HEADERS} ${LWSDK_SOURCES} )
target_include_directories(lwsdk PUBLIC headers ${LIBWEBSOCKETS_INCLUDE_DIRS} )
target_link_libraries(lwsdk ${LIBWEBSOCKETS_LIBRARIES} )

 