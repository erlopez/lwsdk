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
// Being a template, this file is included inline inside the header file

template <typename T> ConcurrentQueue<T>::ConcurrentQueue( int capacity ) noexcept
{
    this->capacity = capacity > 0 ? capacity : 0;
    interrupted = false;

}


template <typename T> void ConcurrentQueue<T>::interrupt()
{
    std::lock_guard<std::mutex> lock(mtx);
    interrupted = true;
    
    cv.notify_all();
}


template <typename T> bool ConcurrentQueue<T>::isEmpty()
{
    std::lock_guard<std::mutex> lock(mtx);
    return items.size() == 0;
}


template <typename T> int ConcurrentQueue<T>::size()
{
    std::lock_guard<std::mutex> lock(mtx);
    return items.size();
}


template <typename T> T ConcurrentQueue<T>::take( bool remove )
{
    std::unique_lock<std::mutex> ulock(mtx);

    // tip: when receiving notification, cv.wait() unblocks when the lambda expression yields true
    auto unblockCondition = [this] { return interrupted || !items.empty(); };

    // Block if needed
    cv.wait( ulock, unblockCondition );

    // Test conditions after waking up
    if ( interrupted )
    {
        throw InterruptedException( "ConcurrentQueue take() interrupted." );
    }

    // Pop value out of queue
    T val = items.front();
    if ( remove )
        items.pop_front();

    cv.notify_all();

    return val;
}


template<typename T> std::optional<T> ConcurrentQueue<T>::take( uint32_t timeoutMsec, bool remove  )
{
    // Fail fast
    if ( timeoutMsec == 0 && items.empty() )
        return std::nullopt;

    std::unique_lock<std::mutex> ulock(mtx);

    // tip: when receiving notification, cv.wait() unblocks when the lambda expression yields true
    auto unblockCondition = [this] { return interrupted || !items.empty(); };

    // Block as needed
    bool timeout = !cv.wait_for( ulock, std::chrono::milliseconds( timeoutMsec ), unblockCondition );

    // Test conditions after waking up
    if ( timeout || interrupted )
        return std::nullopt;

    // Pop value out of queue
    T val = items.front();
    if ( remove )
        items.pop_front();

    cv.notify_all();

    return val;
}


template<typename T> T ConcurrentQueue<T>::take( uint32_t timeoutMsec, const T& timeoutVal, bool remove )
{
    // Fail fast
    if ( timeoutMsec == 0 && items.empty() )
        return timeoutVal;
    
    std::unique_lock<std::mutex> ulock(mtx);

    // tip: when receiving notification, cv.wait() unblocks when the lambda expression yields true
    auto unblockCondition = [this] { return interrupted || !items.empty(); };

    // Block if needed
    bool timeout = !cv.wait_for( ulock, std::chrono::milliseconds( timeoutMsec ), unblockCondition );

    // Test conditions after waking up
    if ( timeout || interrupted )
    {
        //printf("    take() --> %s \n", interrupted ? "INTERRUPTED" : "TIMEOUT" );
        return timeoutVal;
    }

    // Pop value out of queue
    T val = items.front();
    if ( remove )
        items.pop_front();

    cv.notify_all();

    return val;
}


template <typename T> void ConcurrentQueue<T>::offer( T item )
{
    std::unique_lock<std::mutex> ulock(mtx);
    interrupted = false;

    // tip: when receiving notification, cv.wait() unblocks when the lambda expression yields true
    auto unblockCondition = [this] { return interrupted || capacity == 0 || items.size() < (size_t)capacity; };

    // Block if needed
    cv.wait( ulock, unblockCondition );

    // Test conditions after waking up
    if ( interrupted )
    {
        throw InterruptedException( "ConcurrentQueue offer() interrupted." );
    }

    // Push value into queue
    items.push_back( item );
    cv.notify_one();
}


template <typename T> bool ConcurrentQueue<T>::offer( T item, uint32_t timeoutMsec )
{
    std::unique_lock<std::mutex> ulock(mtx);
    interrupted = false;

    // tip: when receiving notification, cv.wait() unblocks when the lambda expression yields true
    auto unblockCondition = [this] { return interrupted || capacity == 0 || items.size() < (size_t)capacity; };

    bool timeout = !cv.wait_for( ulock, std::chrono::milliseconds( timeoutMsec ), unblockCondition );

    // Test conditions after waking up
    if ( timeout || interrupted )
    {
        //printf("    offer() --> %s \n", this->interrupted ? "INTERRUPTED" : "TIMEOUT" );
        //std::this_thread::yield();
        return false;
    }


    // Push value into queue
    items.push_back( item );
    cv.notify_one();


    return true;
}
