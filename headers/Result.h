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
#ifndef RESULT_H
#define RESULT_H

#include <string>
#include <variant>
#include "Exceptions.h"

namespace lwsdk
{
    /**
     * Wraps a value returned by function along with a valid/invalid status.
     * @tparam T Type of the value contained inside the object.
     */
    template <typename T> class Result
    {
        bool valid{false};
        std::variant<std::nullptr_t, T> val{}; // defaults to nullptr

    public:
        /**
         * Creates a result with invalid state and no value
         */
        Result() = default;

        /**
         * Creates a result with a valid or invalid value.
         *
         * For example, consider a function that returns a string (type T is a string),
         * upon success, isValid=true and value="something successful", on error
         * however, you might return a result with no value (ie. isValid=false,
         * value=nullptr) by using the default constructor; or you could also return
         * isValid=false and set value to an error message of why the operation failed.
         * The usage is implementation dependent.
         *
         * @param isValid  Boolean indicating if the given value is valid (ie. good or bad).
         * @param value    A value associated with the valid or invalid state.
         */
        Result( bool isValid, const T& value ) : valid( isValid ), val( value )
        {}

        /**
         * Test if the this instance contains a valid or invalid value
         * @return true if the this instance contains a valid value, false otherwise
         */
        bool isValid();


        /**
         * Returns a reference to the value contained in this instance.
         * If the value is no set a RuntimeException is thrown. Whether the value
         * is set or not in relation to the valid state is implementation dependent.
         *
         * @return Reference to the value contained in this instance.
         * @throw RuntimeException is this instance has no valid value.
         */
        T& value();

    };


    // --- Template method implementations ---


    template<typename T> bool Result<T>::isValid()
    {
        return valid;
    }


    template<typename T> T& Result<T>::value()
    {
        if (std::holds_alternative<std::nullptr_t>(val) )
        {
            throw RuntimeException( " Result::value() - instance contains no value." );
        }
        else
        {
            return std::get<T>(val);
        }
    }



    // --- Other useful types variants ---

    using ResultString = Result<std::string>;

} // namespace lwsdk


#endif //RESULT_H
