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

// Simplistic logger macros
#ifndef LOGGER_H
#define LOGGER_H

#include <cerrno>
#include <cstring>


// TODO: Note all #defines will bleed outside,
//       need to convert macros to functions at some point
namespace lwsdk
{


#ifndef LOGGER_ENABLED
    #define LOGGER_ENABLED 1
#endif

/* Colors */
#define NOC  "\e[0m"    // Default FG/BG

#define FG0  "\e[30m"   // Black
#define FG1  "\e[31m"   // Red
#define FG2  "\e[32m"   // Green
#define FG3  "\e[33m"   // Yellow
#define FG4  "\e[34m"   // Blue
#define FG5  "\e[35m"   // Magenta
#define FG6  "\e[36m"   // Cyan
#define FG7  "\e[37m"   // White
#define FG8  "\e[1;30m" // Gray
#define FG9  "\e[1;31m" // Bright Red
#define FG10 "\e[1;32m" // Bright Green
#define FG11 "\e[1;33m" // Bright Yellow
#define FG12 "\e[1;34m" // Bright Blue
#define FG13 "\e[1;35m" // Bright Magenta
#define FG14 "\e[1;36m" // Bright Cyan
#define FG15 "\e[1;37m" // Bright Cyan

#define BG0  "\e[40m"   // Black
#define BG1  "\e[41m"   // Red
#define BG2  "\e[42m"   // Green
#define BG3  "\e[43m"   // Yellow
#define BG4  "\e[44m"   // Blue
#define BG5  "\e[45m"   // Magenta
#define BG6  "\e[46m"   // Cyan
#define BG7  "\e[47m"   // White

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Note: __MODULE_NAME__ should be defined in the makefile linking up this module; if not defined, fallback to __FILENAME__
//       e.g.  ccflags-y += -D__MODULE_NAME__='"$(MODULE_OUT_NAME)"'
#ifndef __MODULE_NAME__
    #define __MODULE_NAME__  __FILENAME__
#endif

#if LOGGER_ENABLED

    #define trace (fmt, args...) fprintf( stderr, fmt  NOC  "\n", ## args)
    #define traceR(fmt, args...) fprintf( stderr, FG9  fmt  NOC  "\n", ## args)
    #define traceB(fmt, args...) fprintf( stderr, FG12 fmt  NOC  "\n", ## args)
    #define traceG(fmt, args...) fprintf( stderr, FG10 fmt  NOC  "\n", ## args)
    #define traceM(fmt, args...) fprintf( stderr, FG13 fmt  NOC  "\n", ## args)
    #define traceC(fmt, args...) fprintf( stderr, FG14 fmt  NOC  "\n", ## args)
    #define traceY(fmt, args...) fprintf( stderr, FG11 fmt  NOC  "\n", ## args)
    #define traceW(fmt, args...) fprintf( stderr, FG15 fmt  NOC  "\n", ## args)

    // colored
    #define logR(fmt, args...) fprintf( stderr, FG9  "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logB(fmt, args...) fprintf( stderr, FG12 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logG(fmt, args...) fprintf( stderr, FG10 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logM(fmt, args...) fprintf( stderr, FG13 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logC(fmt, args...) fprintf( stderr, FG14 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logY(fmt, args...) fprintf( stderr, FG11 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logW(fmt, args...) fprintf( stderr, FG15 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)

    // normal
    #define logi(fmt, args...) fprintf( stderr, "%s:%d:%s() " fmt "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define logw(fmt, args...) fprintf( stderr, FG12 "%s:%d:%s() " fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define loge(fmt, args...) fprintf( stderr, FG9 "%s:%d:%s() "  fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define fatal(fmt, args...) {fprintf( stderr,  FG9 "%s:%d:%s() "  fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args); exit(-1);}

#else
    #define trace (fmt, args...)
    #define traceR(fmt, args...)
    #define traceB(fmt, args...)
    #define traceG(fmt, args...)
    #define traceM(fmt, args...)
    #define traceC(fmt, args...)
    #define traceY(fmt, args...)
    #define traceW(fmt, args...)

    // colored
    #define logR(fmt, args...)
    #define logB(fmt, args...)
    #define logG(fmt, args...)
    #define logM(fmt, args...)
    #define logC(fmt, args...)
    #define logY(fmt, args...)
    #define logW(fmt, args...)

    #define logi(fmt, args...)
    #define logw(fmt, args...)
    #define loge(fmt, args...) fprintf( stderr,  FG9 "%s:%d:%s() "  fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args)
    #define fatal(fmt, args...) {fprintf( stderr,  FG9 "%s:%d:%s() "  fmt  NOC  "\n", __MODULE_NAME__ , __LINE__, __func__, ## args); exit(-1);}

#endif


} // namespace lwsdk


#endif  /* _H header guard*/
