/**
 * @file ThreadPool.hpp
 * @author Mark Stevens
 * @brief Define the ThreadPool class used to manage a pool of threads for executing methods.
 * @date 2021-01-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Thread.hpp"

/**
 *  @brief Define the ThreadPool class.
 */
class ThreadPool
{
private:
    /**
     *  @brief Maximum number of threads we should allow.
     */
    const int MAX_THREADS = 10;

    /**
     *  @brief Size of the thread pool.
     */
    const int THREAD_POOL_SIZE = 5;

    /**
     * @brief Timeout duration (in milliseconds) for acquiring a semaphore representing thread availability in the thread pool.
     */
    uint32_t _threadPoolAcquireTimeoutInMilisseconds = 5000;

    /**
     *  @brief Threads in the thread pool.
     */
    Thread **_threads = nullptr;

    /**
     *  @brief Counting semaphore used to indicate if a thread is available.
     */
    static SemaphoreHandle_t _threadsAvailable;

public:
    /**
     *  @brief Name of the componet / task for the ThreadPool.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor.
     */
    ThreadPool();

    /**
     *  @brief Destructor.
     */
    ~ThreadPool();

    /**
     *  @brief Use of the the thread to execute a method.
     */
    void Execute(method_t method, void *parameter);

    /**
     *  @brief A call back method to allow a thread to inform the ThreadPool that is has completed a task an is available.
     */
    static void ThreadCompleted();
};