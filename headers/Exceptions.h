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
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

namespace lwsdk
{
    class InterruptedException : public std::exception
    {
        std::string cause;
    public:
        explicit InterruptedException( std::string cause = "InterruptedException" ) : cause( std::move( cause )) {}
        virtual const char *what() const noexcept { return cause.c_str(); }
    };



    class IOException : public std::exception
    {
        std::string cause;
    public:
        explicit IOException( std::string cause = "IOException" ) : cause(std::move( cause )) {}
        virtual const char *what() const noexcept { return cause.c_str(); }
    };



    class RuntimeException : public std::exception
    {
        std::string cause;
    public:
        explicit RuntimeException( std::string cause = "RuntimeException" ) : cause(std::move( cause )) {}
        virtual const char *what() const noexcept { return cause.c_str(); }
    };

} // namespace lwsdk



#endif //EXCEPTIONS_H
