/**
 * @file ThreadPool.cpp
 * @author Mark Stevens
 * @brief Implementation of the ThreadPool class.
 * @date 2021-01-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "sdkconfig.h"

#include "Logging.hpp"
#include "ThreadPool.hpp"
#include "Exceptions/ThreadPoolException.hpp"
#include "Utils.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task for the ThreadPool.
 */
const char *ThreadPool::COMPONENT_NAME = "ThreadPool";

/**
 *  @brief Counting semaphore used to indicate if there are any thread available.
 */
SemaphoreHandle_t ThreadPool::_threadsAvailable = NULL;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor.
 */
ThreadPool::ThreadPool()
{
    TRACE_MESSAGE("ThreadPool: Enter");

    _threads = static_cast<Thread **>(pvPortMalloc(THREAD_POOL_SIZE * sizeof(Thread *)));
    if (_threads == NULL)
    {
        throw new ThreadPoolException("Cannot allocate memory for threads.");
    }
    //
    //  Create the counting semaphore with a maximum count equal to the number of threads in the pool and an initial count of 0.
    //  The count will be incremented as each thread is created and becomes available for use.
    //
    _threadsAvailable = xSemaphoreCreateCounting(THREAD_POOL_SIZE, 0);
    if (_threadsAvailable == NULL)
    {
        throw new ThreadPoolException("Cannot create semaphore for ThreadPool.");
    }

    for (uint8_t index = 0; index < THREAD_POOL_SIZE; index++)
    {
        char *name = static_cast<char *>(pvPortMalloc(15));
        if (name != NULL)
        {
            snprintf(name, 15, "Thread-%02" PRIu8, index);
            _threads[index] = new Thread(name, ThreadPool::ThreadCompleted);
            if (_threads[index])
            {
                xSemaphoreGive(_threadsAvailable);
            }
        }
        else
        {
            throw new ThreadPoolException("Cannot allocate memory for thread names.");
        }
    }

    TRACE_MESSAGE("ThreadPool: Exit");
}

/**
 *  @brief Destructor for the ThreadPool.
 */
ThreadPool::~ThreadPool()
{
    if (_threadsAvailable != NULL)
    {
        for (uint32_t index = 0; index < THREAD_POOL_SIZE; index++)
        {
            if (_threads[index])
            {
                delete _threads[index];
            }
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&_threads));
        vSemaphoreDelete(_threadsAvailable);
        _threadsAvailable = nullptr;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Execute the specified method in the thread pool.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 */
void ThreadPool::Execute(method_t method, void *parameter)
{
    TRACE_MESSAGE("Execute: Enter");

    if (xSemaphoreTake(_threadsAvailable, _threadPoolAcquireTimeoutInMilliseconds / portTICK_PERIOD_MS) == pdTRUE)
    {
        TRACE_MESSAGE("Execute: Searching for an available thread");

        for (uint8_t index = 0; index < THREAD_POOL_SIZE; index++)
        {
            if (_threads[index] && !_threads[index]->IsBusy())
            {
                TRACE_MESSAGE("Execute: Thread %" PRIu8 " available!", index);
                _threads[index]->Execute(method, parameter);
                return;
            }
            TRACE_MESSAGE("Execute: Thread %" PRIu8 " not available", index);
        }
    }

    TRACE_MESSAGE("Execute: No threads available");
    throw ThreadPoolException("Cannot obtain access to an available thread.");
}

/**
 *  @brief Used by the Thread to indicate that it has completed execution of a method and it can be used again.
 */
void ThreadPool::ThreadCompleted()
{
    TRACE_MESSAGE("ThreadCompleted: Enter");

    xSemaphoreGive(_threadsAvailable);

    TRACE_MESSAGE("ThreadCompleted: Exit");
}