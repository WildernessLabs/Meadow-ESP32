/**
 * @file TestThread.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the Thread class.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <atomic>
#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Thread.hpp"
#include "Exceptions/ThreadPoolException.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  [Emulated] marks a test that is suitable for an unattended run under QEMU.  The Thread
 *  class is FreeRTOS tasks and semaphores and touches no hardware, so it needs no board.
 */
#define CLASS_OF_TESTS "[Thread][Emulated]"

/**
 * @brief Milliseconds a method runs for when a test needs to catch the thread working.
 */
#define SLOW_METHOD_DURATION 500

/**
 * @brief Milliseconds to allow a method to start and finish.
 */
#define METHOD_SETTLING_TIME 200

/**
 * @brief Number of times the test method has been called.
 *
 * @note Written by the thread's task and read by the test's, so the counters are atomic.
 */
static std::atomic<uint32_t> _methodCallCount {0};

/**
 * @brief Parameter the test method was given when it was last called.
 */
static std::atomic<void *> _methodParameter {nullptr};

/**
 * @brief Number of times the completion method has been called.
 */
static std::atomic<uint32_t> _completedCallCount {0};

/**
 * @brief Method the tests ask a thread to run.
 *
 * @param parameter Parameter passed through by the thread.
 */
static void CountingMethod(void *parameter)
{
    _methodParameter.store(parameter);
    _methodCallCount.fetch_add(1);
}

/**
 * @brief Method that takes long enough for the test to see the thread working.
 *
 * @param parameter Parameter passed through by the thread.
 */
static void SlowMethod(void *parameter)
{
    (void) parameter;
    vTaskDelay(SLOW_METHOD_DURATION / portTICK_PERIOD_MS);
    _methodCallCount.fetch_add(1);
}

/**
 * @brief Method a thread calls to report that it has finished.
 */
static void CountingCompletionMethod()
{
    _completedCallCount.fetch_add(1);
}

/**
 * @brief Reset the counters the test methods maintain.
 */
static void ResetCounters()
{
    _methodCallCount.store(0);
    _methodParameter.store(nullptr);
    _completedCallCount.store(0);
}

/**
 * @brief Create a thread for a test to use.
 *
 * @note The thread takes ownership of the name, which it releases in its destructor, so
 *       the name is allocated on the heap rather than being a literal.
 *
 * @param name Name to give the thread.
 * @param completionMethod Method the thread calls when it has finished, nullptr for none.
 *
 * @return Pointer to the thread, which the caller owns.
 */
static Thread *CreateThread(const char *name, task_completed_method_t completionMethod)
{
    char *threadName = static_cast<char *>(pvPortMalloc(configMAX_TASK_NAME_LEN));
    TEST_ASSERT_NOT_NULL(threadName);
    snprintf(threadName, configMAX_TASK_NAME_LEN, "%s", name);

    Thread *thread = new Thread(threadName, completionMethod);
    TEST_ASSERT_NOT_NULL(thread);

    return (thread);
}

/**
 * @brief Check that a thread runs the method it is given, with the parameter it is given.
 */
TEST_CASE("Thread executes a method", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestThread", nullptr);

    uint32_t parameter = 0x12345678;
    thread->Execute(CountingMethod, &parameter);

    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(1, _methodCallCount.load());
    TEST_ASSERT_EQUAL_PTR(&parameter, _methodParameter.load());

    delete thread;
}

/**
 * @brief Check that a thread reports itself busy while it is running a method.
 */
TEST_CASE("Thread reports that it is busy", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestBusy", nullptr);

    TEST_ASSERT_FALSE(thread->IsBusy());

    thread->Execute(SlowMethod, nullptr);
    TEST_ASSERT_TRUE(thread->IsBusy());

    //
    //  The method outlives this check, so the thread is still working when it is made.
    //
    vTaskDelay((SLOW_METHOD_DURATION / 2) / portTICK_PERIOD_MS);
    TEST_ASSERT_TRUE(thread->IsBusy());

    vTaskDelay((SLOW_METHOD_DURATION + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_FALSE(thread->IsBusy());
    TEST_ASSERT_EQUAL(1, _methodCallCount.load());

    delete thread;
}

/**
 * @brief Check that a thread reports the name, method and parameter it was given.
 */
TEST_CASE("Thread reports its name, method and parameter", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestDetails", nullptr);

    TEST_ASSERT_EQUAL_STRING("TestDetails", thread->GetTaskName());
    //
    //  Nothing has been asked of the thread yet, so it is holding no work.
    //
    TEST_ASSERT_NULL(thread->GetMethod());
    TEST_ASSERT_NULL(thread->GetParameter());

    uint32_t parameter = 0xa5a5a5a5;
    thread->Execute(SlowMethod, &parameter);

    TEST_ASSERT_EQUAL_PTR(SlowMethod, thread->GetMethod());
    TEST_ASSERT_EQUAL_PTR(&parameter, thread->GetParameter());

    //
    //  The method and parameter are cleared once the method has finished.
    //
    vTaskDelay((SLOW_METHOD_DURATION + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_NULL(thread->GetMethod());
    TEST_ASSERT_NULL(thread->GetParameter());

    delete thread;
}

/**
 * @brief Check that a thread calls the completion method when it has finished.
 */
TEST_CASE("Thread reports that it has completed", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestComplete", CountingCompletionMethod);

    TEST_ASSERT_EQUAL(0, _completedCallCount.load());

    thread->Execute(CountingMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(1, _methodCallCount.load());
    TEST_ASSERT_EQUAL(1, _completedCallCount.load());

    //
    //  The thread is available again, so it takes a second method and reports that one too.
    //
    thread->Execute(CountingMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(2, _methodCallCount.load());
    TEST_ASSERT_EQUAL(2, _completedCallCount.load());

    delete thread;
}

/**
 * @brief Check that a thread refuses a method while it is running one.
 *
 *  The exception is thrown by value, so a catch by reference matches it.  This is what
 *  WiFiRequestHandler::DispatchRequest relies on to answer ThreadPoolIsFull rather than
 *  letting the exception reach std::terminate.
 *
 *  The thread also has to survive being refused.  Execute holds a mutex while it decides
 *  whether the thread is free, and TaskCompleted waits on that same mutex without a
 *  timeout, so a thread that kept the mutex on the way out could never report that it had
 *  finished.  The checks after the catch are what would fail if it did.
 */
TEST_CASE("Thread refuses a method while it is busy", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestBusyThrow", CountingCompletionMethod);

    thread->Execute(SlowMethod, nullptr);
    TEST_ASSERT_TRUE(thread->IsBusy());

    bool caught = false;
    try
    {
        thread->Execute(CountingMethod, nullptr);
    }
    catch (const ThreadPoolException &exception)
    {
        caught = true;
        printf("Refused as expected: %s\n", exception.what());
    }
    TEST_ASSERT_TRUE_MESSAGE(caught, "A busy thread accepted a second method.");

    //
    //  The refused method was never run, and the first one still finishes and reports.
    //
    vTaskDelay((SLOW_METHOD_DURATION + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(1, _methodCallCount.load());
    TEST_ASSERT_EQUAL(1, _completedCallCount.load());
    TEST_ASSERT_FALSE_MESSAGE(thread->IsBusy(), "The thread did not recover from refusing a method.");

    //
    //  A thread that recovered takes work again.
    //
    thread->Execute(CountingMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(2, _methodCallCount.load());

    delete thread;
}

/**
 * @brief Check that TryExecute claims an idle thread and leaves a busy one untouched.
 *
 *  TryExecute is what the pool uses to check a thread is idle and claim it in one step.
 *  A refusal must report false rather than throw, and must not replace the method or
 *  parameter the thread is already running with.
 */
TEST_CASE("Thread TryExecute claims an idle thread and refuses a busy one", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestTryExecute", CountingCompletionMethod);
    int firstParameter = 1;
    int secondParameter = 2;

    TEST_ASSERT_TRUE_MESSAGE(thread->TryExecute(SlowMethod, &firstParameter), "An idle thread refused a method.");
    TEST_ASSERT_TRUE(thread->IsBusy());

    TEST_ASSERT_FALSE_MESSAGE(thread->TryExecute(CountingMethod, &secondParameter), "A busy thread accepted a second method.");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&firstParameter, thread->GetParameter(), "A refused claim replaced the running method's parameter.");
    TEST_ASSERT_TRUE_MESSAGE(thread->GetMethod() == SlowMethod, "A refused claim replaced the running method.");

    vTaskDelay((SLOW_METHOD_DURATION + METHOD_SETTLING_TIME) / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL_MESSAGE(1, _methodCallCount.load(), "The refused method ran.");
    TEST_ASSERT_EQUAL(1, _completedCallCount.load());
    TEST_ASSERT_FALSE(thread->IsBusy());

    TEST_ASSERT_TRUE_MESSAGE(thread->TryExecute(CountingMethod, &secondParameter), "The thread did not take work again once idle.");
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(2, _methodCallCount.load());
    TEST_ASSERT_EQUAL_PTR(&secondParameter, _methodParameter.load());

    delete thread;
}

/**
 * @brief Method that throws, to check the thread survives it.
 *
 * @param parameter Parameter passed through by the thread.
 */
static void ThrowingMethod(void *parameter)
{
    (void) parameter;
    _methodCallCount.fetch_add(1);
    throw ThreadPoolException("A method that always throws.");
}

/**
 * @brief Check that a thread is handed back when the method it ran threw.
 *
 *  Nothing above the task can catch the exception, so without a catch in the task it
 *  reaches std::terminate and the board resets (issue #229).  The thread must also report
 *  that it finished, or the pool never gets its token back and the thread stays busy.
 */
TEST_CASE("Thread is handed back when its method throws", CLASS_OF_TESTS)
{
    ResetCounters();

    Thread *thread = CreateThread("TestThrow", CountingCompletionMethod);

    thread->Execute(ThrowingMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(1, _methodCallCount.load());
    TEST_ASSERT_EQUAL_MESSAGE(1, _completedCallCount.load(), "The thread did not report that it had finished.");
    TEST_ASSERT_FALSE_MESSAGE(thread->IsBusy(), "The thread is still busy after its method threw.");

    //
    //  A method that is nullptr has nothing to run but is still a claim that must be completed.
    //
    thread->Execute(nullptr, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(2, _completedCallCount.load());
    TEST_ASSERT_FALSE(thread->IsBusy());

    //
    //  It runs another method afterwards.
    //
    thread->Execute(CountingMethod, nullptr);
    vTaskDelay(METHOD_SETTLING_TIME / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(2, _methodCallCount.load());
    TEST_ASSERT_EQUAL(3, _completedCallCount.load());

    delete thread;
}
