/**
 * @file TestThreadPool.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the ThreadPool class.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <atomic>
#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "Globals.hpp"
#include "ThreadPool.hpp"
#include "Exceptions/ThreadPoolException.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  [Emulated] marks a test that is suitable for an unattended run under QEMU.  The
 *  ThreadPool is FreeRTOS tasks and semaphores and touches no hardware, so it needs no
 *  board.
 */
#define CLASS_OF_TESTS "[ThreadPool][Emulated]"

/**
 * @brief Number of threads the pool holds.
 *
 * @note THREAD_POOL_SIZE is private to the ThreadPool, so the value is repeated here.
 *       The test that fills the pool checks that this is still the right number.
 */
#define EXPECTED_THREAD_POOL_SIZE 5

/**
 * @brief Milliseconds a method holds a thread for while the pool is being filled.
 */
#define OCCUPYING_METHOD_DURATION 2000

/**
 * @brief Milliseconds to allow a method to start and finish.
 */
#define METHOD_SETTLING_TIME 500

/**
 * @brief Milliseconds the pool waits for a thread before it gives up.
 *
 * @note This mirrors _threadPoolAcquireTimeoutInMilliseconds, which is private.
 */
#define POOL_ACQUIRE_TIMEOUT 5000

/**
 * @brief Number of methods that have started.
 *
 * @note Written by the pool's tasks and read by the test's, so the counters are atomic.
 */
static std::atomic<uint32_t> _methodsStarted {0};

/**
 * @brief Number of methods that have finished.
 */
static std::atomic<uint32_t> _methodsFinished {0};

/**
 * @brief How long the occupying method should hold its thread for, in milliseconds.
 */
static std::atomic<uint32_t> _occupyingMethodDuration {OCCUPYING_METHOD_DURATION};

/**
 * @brief Method that returns immediately.
 *
 * @param parameter Parameter passed through by the thread.
 */
static void QuickMethod(void *parameter)
{
    (void) parameter;
    _methodsStarted.fetch_add(1);
    _methodsFinished.fetch_add(1);
}

/**
 * @brief Method that holds its thread long enough for the test to fill the pool.
 *
 * @param parameter Parameter passed through by the thread.
 */
static void OccupyingMethod(void *parameter)
{
    (void) parameter;
    _methodsStarted.fetch_add(1);
    vTaskDelay(_occupyingMethodDuration.load() / portTICK_PERIOD_MS);
    _methodsFinished.fetch_add(1);
}

/**
 * @brief Reset the counters the test methods maintain.
 */
static void ResetCounters()
{
    _methodsStarted.store(0);
    _methodsFinished.store(0);
    _occupyingMethodDuration.store(OCCUPYING_METHOD_DURATION);
}

/**
 * @brief Check that the pool runs a method that is given to it.
 */
TEST_CASE("ThreadPool executes a method", CLASS_OF_TESTS)
{
    ResetCounters();

    ThreadPool *pool = new ThreadPool();
    TEST_ASSERT_NOT_NULL(pool);

    pool->Execute(QuickMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(1, _methodsStarted.load());
    TEST_ASSERT_EQUAL(1, _methodsFinished.load());

    delete pool;
}

/**
 * @brief Check that the pool runs as many methods at once as it holds threads.
 */
TEST_CASE("ThreadPool runs several methods at once", CLASS_OF_TESTS)
{
    ResetCounters();

    ThreadPool *pool = new ThreadPool();
    TEST_ASSERT_NOT_NULL(pool);

    for (int index = 0; index < EXPECTED_THREAD_POOL_SIZE; index++)
    {
        pool->Execute(OccupyingMethod, nullptr);
    }

    //
    //  Every method should be running at this point, none of them finished.  If the pool
    //  were running them one after another only the first would have started.
    //
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(EXPECTED_THREAD_POOL_SIZE, _methodsStarted.load());
    TEST_ASSERT_EQUAL(0, _methodsFinished.load());

    vTaskDelay((OCCUPYING_METHOD_DURATION + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(EXPECTED_THREAD_POOL_SIZE, _methodsFinished.load());

    delete pool;
}

/**
 * @brief Check that the pool refuses work when every thread it holds is busy.
 *
 *  The exception is thrown by value, so a catch by reference matches it.  This is what
 *  WiFiRequestHandler::DispatchRequest relies on to answer ThreadPoolIsFull rather than
 *  letting the exception reach std::terminate.
 */
TEST_CASE("ThreadPool refuses work when it is full", CLASS_OF_TESTS)
{
    ResetCounters();
    //
    //  The methods have to outlast the time the pool spends waiting for a thread,
    //  otherwise one comes free while the extra request is still waiting for one.
    //
    _occupyingMethodDuration.store(POOL_ACQUIRE_TIMEOUT + (2 * METHOD_SETTLING_TIME));

    ThreadPool *pool = new ThreadPool();
    TEST_ASSERT_NOT_NULL(pool);

    for (int index = 0; index < EXPECTED_THREAD_POOL_SIZE; index++)
    {
        pool->Execute(OccupyingMethod, nullptr);
    }

    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL_MESSAGE(EXPECTED_THREAD_POOL_SIZE, _methodsStarted.load(), "The pool does not hold the number of threads this test expects.");

    //
    //  The pool is full, so this request waits for a thread and then gives up.
    //
    bool caught = false;
    try
    {
        pool->Execute(QuickMethod, nullptr);
    }
    catch (const ThreadPoolException &exception)
    {
        caught = true;
        printf("Refused as expected: %s\n", exception.what());
    }
    TEST_ASSERT_TRUE_MESSAGE(caught, "A full thread pool accepted more work.");

    //
    //  The refused method was never run.
    //
    TEST_ASSERT_EQUAL(EXPECTED_THREAD_POOL_SIZE, _methodsStarted.load());

    vTaskDelay((_occupyingMethodDuration.load() + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(EXPECTED_THREAD_POOL_SIZE, _methodsFinished.load());

    delete pool;
}

/**
 * @brief Check that a thread goes back into the pool once its method has finished.
 */
TEST_CASE("ThreadPool reuses a thread once its method has finished", CLASS_OF_TESTS)
{
    ResetCounters();

    ThreadPool *pool = new ThreadPool();
    TEST_ASSERT_NOT_NULL(pool);

    //
    //  Run more methods than the pool holds threads, a few at a time.  This can only
    //  finish if threads are returned to the pool and handed out again.
    //
    const uint32_t rounds = 3;
    const uint32_t expected = rounds * EXPECTED_THREAD_POOL_SIZE;

    for (uint32_t round = 0; round < rounds; round++)
    {
        for (int index = 0; index < EXPECTED_THREAD_POOL_SIZE; index++)
        {
            pool->Execute(QuickMethod, nullptr);
        }
        vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    }

    TEST_ASSERT_EQUAL(expected, _methodsStarted.load());
    TEST_ASSERT_EQUAL(expected, _methodsFinished.load());

    delete pool;
}

/**
 * @brief Number of methods each submitting task hands to the pool in the concurrency test.
 */
#define CONCURRENT_SUBMISSIONS 5000

/**
 * @brief Number of tasks submitting to the pool at the same time, spread across both cores.
 */
#define CONCURRENT_SUBMITTERS 4

/**
 * @brief Milliseconds allowed for the submitting tasks to finish.
 */
#define CONCURRENT_SUBMISSION_TIMEOUT 120000

/**
 * @brief Everything a submitting task needs, and what it reports back.
 */
struct SubmitterContext
{
    /**
     * @brief Pool the task submits to.
     */
    ThreadPool *Pool;

    /**
     * @brief Number of submissions the pool refused with a ThreadPoolException.
     */
    std::atomic<uint32_t> Refusals {0};

    /**
     * @brief Given once by each task when it has made all of its submissions.
     */
    SemaphoreHandle_t Finished;
};

/**
 * @brief Task that hands CONCURRENT_SUBMISSIONS quick methods to the pool as fast as it can.
 *
 * @param parameter SubmitterContext shared with the test and the other submitting task.
 */
static void SubmitterTask(void *parameter)
{
    SubmitterContext *context = static_cast<SubmitterContext *>(parameter);
    for (uint32_t index = 0; index < CONCURRENT_SUBMISSIONS; index++)
    {
        try
        {
            context->Pool->Execute(QuickMethod, nullptr);
        }
        catch (const ThreadPoolException &e)
        {
            //
            //  A refusal also loses the pool a token, so carrying on would soon leave every
            //  submission waiting out the acquire timeout.  One is enough to fail the test.
            //
            context->Refusals.fetch_add(1);
            break;
        }
    }
    xSemaphoreGive(context->Finished);
    vTaskDelete(nullptr);
}

/**
 * @brief Check that two tasks submitting at once, one on each core, are never refused.
 *
 *  Holding a token from the pool's counting semaphore means a thread is free, so a
 *  correct pool never refuses these submissions.  Choosing a thread used to be a check
 *  followed by a separate claim, so two submitters could pick the same free thread and
 *  the second was refused with "Attempt to execute method on a busy thread", losing its
 *  token as well.
 */
TEST_CASE("ThreadPool gives concurrent callers separate threads", CLASS_OF_TESTS)
{
    ResetCounters();

    //
    //  Static so a submitting task that outlives a failed test cannot write to a dead stack frame.
    //
    static SubmitterContext context;
    context.Refusals.store(0);
    context.Pool = new ThreadPool();
    TEST_ASSERT_NOT_NULL(context.Pool);
    context.Finished = xSemaphoreCreateCounting(CONCURRENT_SUBMITTERS, 0);
    TEST_ASSERT_NOT_NULL(context.Finished);

    for (BaseType_t core = 0; core < CONCURRENT_SUBMITTERS; core++)
    {
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(SubmitterTask, "Submitter", 4096, &context, PRIORITY_THREAD_POOL_TASK, nullptr, core % 2));
    }
    for (uint32_t submitter = 0; submitter < CONCURRENT_SUBMITTERS; submitter++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(context.Finished, CONCURRENT_SUBMISSION_TIMEOUT / portTICK_PERIOD_MS), "A submitting task did not finish.");
    }
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    const uint32_t refusals = context.Refusals.load();
    printf("%" PRIu32 " of %" PRIu32 " concurrent submissions refused.\n", refusals, static_cast<uint32_t>(CONCURRENT_SUBMITTERS * CONCURRENT_SUBMISSIONS));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, refusals, "The pool refused a submission while it held a free thread.");
    TEST_ASSERT_EQUAL(CONCURRENT_SUBMITTERS * CONCURRENT_SUBMISSIONS, _methodsFinished.load());

    vSemaphoreDelete(context.Finished);
    delete context.Pool;
}
