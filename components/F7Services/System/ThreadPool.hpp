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
    /**
     * @brief Test access to the threads and the token semaphore (issue #229).
     */
    friend class TestThreadPoolTokens;

private:
    /**
     *  @brief Size of the thread pool.
     */
    static constexpr int THREAD_POOL_SIZE = 5;

    /**
     * @brief Timeout duration (in milliseconds) for acquiring a semaphore representing thread availability in the thread pool.
     */
    uint32_t _threadPoolAcquireTimeoutInMilliseconds = 5000;

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
     *  @brief Name of the component / task for the ThreadPool.
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
     * @brief Deleted copy constructor, a copy would share and then double free the owned storage.
     */
    ThreadPool(const ThreadPool &other) = delete;

    /**
     * @brief Deleted copy assignment operator, a copy would share and then double free the owned storage.
     */
    ThreadPool &operator=(const ThreadPool &other) = delete;

    /**
     * @brief Deleted move constructor to prevent moving.
     */
    ThreadPool(ThreadPool &&other) = delete;

    /**
     * @brief Deleted move assignment operator to prevent move assignment.
     */
    ThreadPool &operator=(ThreadPool &&other) = delete;

    /**
     *  @brief Use of the the thread to execute a method.
     */
    void Execute(method_t method, void *parameter);

    /**
     *  @brief A call back method to allow a thread to inform the ThreadPool that is has completed a task an is available.
     */
    static void ThreadCompleted();
};