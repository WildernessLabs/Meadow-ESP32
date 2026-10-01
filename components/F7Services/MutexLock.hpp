/**
 * @file MutexLock.hpp
 * @author Mark Stevens
 * @brief Scoped holder for a FreeRTOS mutex.
 * @date 2026-10-01
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 *  @brief Holds a FreeRTOS mutex for the lifetime of the object.
 *
 *  Giving the mutex back in the destructor means it is released on every path out of the
 *  scope, including an early return or an exception thrown while it is held, such as
 *  std::bad_alloc from a container or a std::string copy.
 */
class MutexLock
{
public:
    /**
     *  @brief Take the mutex, waiting for as long as it takes.
     *
     *  @param mutex
     *      Mutex to hold.
     */
    explicit MutexLock(SemaphoreHandle_t mutex) : _mutex(mutex)
    {
        xSemaphoreTake(_mutex, portMAX_DELAY);
    }

    /**
     *  @brief Give the mutex back.
     */
    ~MutexLock()
    {
        xSemaphoreGive(_mutex);
    }

    /**
     *  @brief Deleted copy constructor, a copy would give the mutex back twice.
     */
    MutexLock(const MutexLock &other) = delete;

    /**
     *  @brief Deleted copy assignment operator, a copy would give the mutex back twice.
     */
    MutexLock &operator=(const MutexLock &other) = delete;

private:
    /**
     *  @brief Mutex being held.
     */
    SemaphoreHandle_t _mutex;
};

/**
 *  @brief Holds a FreeRTOS recursive mutex for the lifetime of the object.
 *
 *  The same as MutexLock, for a mutex made with xSemaphoreCreateRecursiveMutex, which the
 *  task already holding it may take again.
 */
class RecursiveMutexLock
{
public:
    /**
     *  @brief Take the mutex, waiting for as long as it takes.
     *
     *  @param mutex
     *      Recursive mutex to hold.
     */
    explicit RecursiveMutexLock(SemaphoreHandle_t mutex) : _mutex(mutex)
    {
        xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }

    /**
     *  @brief Give the mutex back once.
     */
    ~RecursiveMutexLock()
    {
        xSemaphoreGiveRecursive(_mutex);
    }

    /**
     *  @brief Deleted copy constructor, a copy would give the mutex back twice.
     */
    RecursiveMutexLock(const RecursiveMutexLock &other) = delete;

    /**
     *  @brief Deleted copy assignment operator, a copy would give the mutex back twice.
     */
    RecursiveMutexLock &operator=(const RecursiveMutexLock &other) = delete;

private:
    /**
     *  @brief Mutex being held.
     */
    SemaphoreHandle_t _mutex;
};
