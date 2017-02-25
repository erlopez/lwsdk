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
#ifndef CONCURRENTQUEUE_H
#define CONCURRENTQUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <utility>
#include "Exceptions.h"
#include "Result.h"

namespace lwsdk
{
    /**
     * Synchronized queue for exchanging data between threads.
     * @tparam T Type of the data to exchange
     */
    template <typename T> class ConcurrentQueue
    {
        std::deque<T> items;
        std::condition_variable cv;
        std::mutex mtx;
        std::atomic_bool interrupted;
        int capacity;

    public:
        /**
         * Creates an unbounded queue that grows as needed.
         */
        ConcurrentQueue() noexcept : ConcurrentQueue(0)  {}

        /**
         * Creates a fixed-capacity queue. When queue is full, the thread putting data
         * into the queue will block until space becomes available.
         *
         * @param capacity Maximum number of items the queue can hold. If this value is
         *                 0 (or negative) and unbounded queue is created.
         */
        explicit ConcurrentQueue( int capacity ) noexcept;

        /**
         * Wake all threads waiting on the queue. All sleeping threads waiting on the queue
         * will wake up via an InterruptedException. This functionality is typically used
         * to notify threads to check for application changing conditions and determine
         * when to exit to allow the application to dispose cleanly.
         *
         * The queue remains in interrupted state until offer() is called again.
         */
        void interrupt();

        /**
         * Test if the queue is empty.
         * @return true if the queue has contents, false otherwise.
         */
        bool isEmpty();

        /**
         * Return the current number of items in the queue.
         * @return The current number of items in the queue.
         */
        int size();

        /**
         * Returns the next item in the queue. If the queue is empty, the thread blocks
         * until an items becomes available.
         *
         * @param remove True removes the item, false leaves it in the queue.
         * @return Item in the front of the queue (oldest).
         *
         * @throw InterruptedException to wake up blocked threads when interrupt() is called
         *        by another thread.
         */
        T take( bool remove = true );

        /**
         * Returns the next item in the queue. If the queue is empty, the thread blocks
         * for the given number of *timeoutMsec* ms.
         *
         * Invoking interrupt() by another thread will cause this function to timeout even if
         * the given timeoutMsec has not been reached.
         *
         * Example for processing returned optional value:
         *
         *        auto var = take(100);
         *        if ( var.isValid() )
         *           doSomething( var.value() );
         *
         * @param timeoutMsec Maximum number of milliseconds to wait for new data. Set this
         *                    value to 0 for non-blocking behaviour.
         * @param remove      True removes the item, false leaves it in the queue.
         *
         * @return Item in the front of the queue (oldest). An invalid value if the operation
         *         was interrupted or timed out.
         */
        Result<T>  take(uint32_t timeoutMsec, bool remove = true );

        /**
         * Returns the next item in the queue. If the queue is empty, the thread blocks
         * for the given number of *timeoutMsec* ms and returns the given timeout value.
         *
         * Invoking interrupt() by another thread will cause this function to timeout even if
         * the given timeoutMsec has not been reached.
         *
         * @param timeoutMsec Maximum number of milliseconds to wait for new data. Set this
         *                    value to 0 for non-blocking behaviour.
         * @param timeoutVal  Value to return if the operation times out.
         * @param remove      True removes the item, false leaves it in the queue.
         *
         * @return Item in the front of the queue (oldest) or timeoutVal if the timeoutMsec
         *         period was reached or the operation was interrupted.
         * .
         */
        T take(uint32_t timeoutMsec, const T& timeoutVal, bool remove = true );


        /**
         * Puts a new item at the end of the queue. If the queue has reached its maximum capacity
         * (if any was specified), the thread blocks until space becomes available.
         *
         * @param item New value to add to the end of the queue.
         *
         * @throw InterruptedException to wake up blocked threads when interrupt() is called
         *        by another thread.
         */
        void offer( T item );

        /**
         * Puts a new item at the end of the queue. If the queue has reached its maximum capacity
         * (if any was specified), the thread blocks for the given number of timeoutMsec waiting
         * for space to become available. If no queue capacity was specified, the new value is
         * simply added (as long as there is free heap memory available).
         *
         * Invoking interrupt() by another thread will cause this function to timeout even if
         * the given timeoutMsec has not been reached.
         *
         * @param item New value to add to the end of the queue.
         * @param timeoutMsec Maximum number of milliseconds to wait for space to become available.
         *                    Set this value to 0 for non-blocking behaviour.
         *
         * @return True is the item was added, false if the operation timed out or was interrupted.
         */
        bool offer( T item, uint32_t timeoutMsec );
    };

    #include "ConcurrentQueue.tpp"

} // namespace lwsdk


#endif //CONCURRENTQUEUE_H
