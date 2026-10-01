/*
 *  TestAddrInfoHandles.cpp
 *
 *  Tests for the table of handles GetAddrInfo gives the STM32 in place of addrinfo
 *  pointers (issue #168).
 */

#include "sdkconfig.h"

#include <atomic>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"

#include "WiFiRequestHandler.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[AddrInfo][Emulated]"

/**
 * @brief Number of tasks issuing and taking back handles at once, split across both cores.
 */
#define HANDLE_TASKS 4

/**
 * @brief Number of handles each task issues and takes back.
 */
#define HANDLES_PER_TASK 5000

/**
 * @brief Milliseconds to wait for the tasks to finish.
 */
#define HANDLE_TASKS_TIMEOUT 60000

/**
 * @brief Gives the tests access to WiFiRequestHandler's private handle table.
 */
class TestAddrInfoHandles
{
public:
    /**
     * @brief Call the private AddAddrInfoHandle method.
     *
     * @param addressInformation Pointer to record.
     * @return The handle issued for it.
     */
    static uint32_t Add(struct addrinfo * const addressInformation)
    {
        return (WiFiRequestHandler::AddAddrInfoHandle(addressInformation));
    }

    /**
     * @brief Call the private TakeAddrInfoHandle method.
     *
     * @param handle Handle to take back.
     * @return The pointer recorded under it, or nullptr if it is not held.
     */
    static struct addrinfo *Take(const uint32_t handle)
    {
        return (WiFiRequestHandler::TakeAddrInfoHandle(handle));
    }

    /**
     * @brief Number of handles the table currently holds.
     *
     * @return The size of the table.
     */
    static size_t Count()
    {
        return (WiFiRequestHandler::_addrInfoHandles.size());
    }
};

/**
 * @brief State shared between the test and the tasks it starts.
 */
struct HandleTaskContext
{
    /**
     * @brief Takes that returned something other than the pointer the task recorded.
     */
    std::atomic<uint32_t> WrongPointers {0};

    /**
     * @brief Second takes of the same handle that still found something.
     */
    std::atomic<uint32_t> RepeatTakes {0};

    /**
     * @brief Given once by each task when it has finished.
     */
    SemaphoreHandle_t Finished;

    /**
     * @brief Taken by each task before it starts, and given by the test once every task
     *        exists, so they all run at once.  Without it a task at a higher priority than
     *        the test runs to completion before the test creates the next one.
     */
    SemaphoreHandle_t Start;
};

/**
 * @brief Arguments for one task.
 */
struct HandleTaskArguments
{
    /**
     * @brief Context shared by every task.
     */
    HandleTaskContext *Context;

    /**
     * @brief Index of the task, used to make the pointers it records unique to it.
     */
    uint32_t Index;
};

/**
 * @brief Issue and take back HANDLES_PER_TASK handles as fast as possible.
 *
 *  The pointers recorded are never dereferenced by the table, so they need only be
 *  distinct and non-null: each task uses its own range.
 *
 * @param parameter HandleTaskArguments for this task.
 */
static void HandleTask(void *parameter)
{
    HandleTaskArguments *arguments = static_cast<HandleTaskArguments *>(parameter);
    HandleTaskContext *context = arguments->Context;
    const uintptr_t base = 0x10000000u + (arguments->Index * 0x01000000u);
    xSemaphoreTake(context->Start, portMAX_DELAY);

    for (uint32_t iteration = 0; iteration < HANDLES_PER_TASK; iteration++)
    {
        struct addrinfo * const recorded = reinterpret_cast<struct addrinfo *>(base + ((iteration + 1) * 4));
        const uint32_t handle = TestAddrInfoHandles::Add(recorded);
        if (TestAddrInfoHandles::Take(handle) != recorded)
        {
            context->WrongPointers.fetch_add(1);
        }
        if (TestAddrInfoHandles::Take(handle) != nullptr)
        {
            context->RepeatTakes.fetch_add(1);
        }
    }
    xSemaphoreGive(context->Finished);
    vTaskDelete(nullptr);
}

/**
 * @brief Check that several tasks on both cores can issue and take back addrinfo handles
 *        at once without corrupting the table or getting each other's pointers.
 *
 *  GetAddrInfo and FreeAddrInfo run on thread pool workers, and the WiFi task hands the
 *  next request to the pool without waiting for the last one, so they change the table
 *  from several tasks at once.  The table and the counter that issues handles used to have
 *  no lock (issue #168).
 */
TEST_CASE("AddrInfo handles can be issued and taken back from several tasks at once", CLASS_OF_TESTS)
{
    const size_t heldAtStart = TestAddrInfoHandles::Count();

    //
    //  Static so a task that outlives a failed test cannot write to a dead stack frame.
    //
    static HandleTaskContext context;
    static HandleTaskArguments arguments[HANDLE_TASKS];
    context.WrongPointers.store(0);
    context.RepeatTakes.store(0);
    context.Finished = xSemaphoreCreateCounting(HANDLE_TASKS, 0);
    TEST_ASSERT_NOT_NULL(context.Finished);
    context.Start = xSemaphoreCreateCounting(HANDLE_TASKS, 0);
    TEST_ASSERT_NOT_NULL(context.Start);

    for (uint32_t index = 0; index < HANDLE_TASKS; index++)
    {
        arguments[index].Context = &context;
        arguments[index].Index = index;
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(HandleTask, "AddrInfoHandles", 4096, &arguments[index], tskIDLE_PRIORITY + 5, nullptr, index % 2));
    }
    for (uint32_t index = 0; index < HANDLE_TASKS; index++)
    {
        xSemaphoreGive(context.Start);
    }
    for (uint32_t index = 0; index < HANDLE_TASKS; index++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(context.Finished, HANDLE_TASKS_TIMEOUT / portTICK_PERIOD_MS), "A task issuing handles did not finish.");
    }

    printf("%" PRIu32 " wrong pointers and %" PRIu32 " repeat takes in %" PRIu32 " handles.\n", context.WrongPointers.load(), context.RepeatTakes.load(), static_cast<uint32_t>(HANDLE_TASKS * HANDLES_PER_TASK));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, context.WrongPointers.load(), "A handle gave back a pointer other than the one recorded under it.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, context.RepeatTakes.load(), "A handle could be taken back twice.");
    TEST_ASSERT_EQUAL_MESSAGE(heldAtStart, TestAddrInfoHandles::Count(), "The table holds handles that were taken back.");

    vSemaphoreDelete(context.Finished);
    vSemaphoreDelete(context.Start);
}
