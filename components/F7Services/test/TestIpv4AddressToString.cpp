/*
 *  TestIpv4AddressToString.cpp
 *
 *  Tests that IPv4 addresses are formatted without a buffer shared between callers (issue #171).
 */

#include "sdkconfig.h"

#include <atomic>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "unity.h"

#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Utils][Emulated]"

/**
 * @brief Number of addresses each formatting task formats.
 */
#define FORMAT_ITERATIONS 20000

/**
 * @brief Milliseconds to wait for the formatting tasks to finish.
 */
#define FORMAT_TIMEOUT 60000

/**
 * @brief An address in network byte order, from its four parts in the order they are written.
 *
 * @param first First part.
 * @param second Second part.
 * @param third Third part.
 * @param fourth Fourth part.
 *
 * @return The address as in_addr::s_addr holds it.
 */
static uint32_t MakeAddress(const uint8_t first, const uint8_t second, const uint8_t third, const uint8_t fourth)
{
    return (htonl((static_cast<uint32_t>(first) << 24) | (static_cast<uint32_t>(second) << 16) | (static_cast<uint32_t>(third) << 8) | fourth));
}

TEST_CASE("Ipv4AddressToString formats addresses in dotted decimal", CLASS_OF_TESTS)
{
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", Utils::Ipv4AddressToString(0).c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.65", Utils::Ipv4AddressToString(MakeAddress(192, 168, 1, 65)).c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", Utils::Ipv4AddressToString(MakeAddress(255, 255, 255, 0)).c_str());
    //
    //  The longest address there is, which fills INET_ADDRSTRLEN exactly.
    //
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", Utils::Ipv4AddressToString(MakeAddress(255, 255, 255, 255)).c_str());
}

/**
 * @brief State shared between the test and its formatting tasks.
 */
struct FormatContext
{
    /**
     * @brief Address each task formats, one per task.
     */
    uint32_t Addresses[2];

    /**
     * @brief Text each task expects, one per task.
     */
    const char *Expected[2];

    /**
     * @brief Index of the next task to claim its address.
     */
    std::atomic<uint32_t> NextTask;

    /**
     * @brief Number of results that were not the task's own address.
     */
    std::atomic<uint32_t> WrongResults;

    /**
     * @brief Given once per task to start both together.
     */
    SemaphoreHandle_t Start;

    /**
     * @brief Given by each task when it has finished.
     */
    SemaphoreHandle_t Finished;
};

/**
 * @brief Format this task's address repeatedly, counting any result that is not that address.
 *
 * @param parameter FormatContext shared with the test.
 */
static void FormatTask(void *parameter)
{
    FormatContext *context = static_cast<FormatContext *>(parameter);
    const uint32_t task = context->NextTask.fetch_add(1);
    xSemaphoreTake(context->Start, portMAX_DELAY);
    for (uint32_t iteration = 0; iteration < FORMAT_ITERATIONS; iteration++)
    {
        if (Utils::Ipv4AddressToString(context->Addresses[task]) != context->Expected[task])
        {
            context->WrongResults.fetch_add(1);
        }
    }
    xSemaphoreGive(context->Finished);
    vTaskDelete(nullptr);
}

/**
 * @brief Check that two tasks formatting different addresses at once each get their own.
 *
 *  inet_ntoa formats into one static buffer, so a task formatting one address while another
 *  task formats a second can be handed the second (issue #171).  The POSIX handlers run on
 *  several thread pool workers and log the addresses they are given this way.
 */
TEST_CASE("Ipv4AddressToString gives each of two tasks its own address", CLASS_OF_TESTS)
{
    //
    //  Static so a task that outlives a failed test cannot write to a dead stack frame.
    //
    static FormatContext context;
    context.Addresses[0] = MakeAddress(10, 0, 0, 1);
    context.Addresses[1] = MakeAddress(255, 255, 255, 254);
    context.Expected[0] = "10.0.0.1";
    context.Expected[1] = "255.255.255.254";
    context.NextTask.store(0);
    context.WrongResults.store(0);
    context.Start = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(context.Start);
    context.Finished = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(context.Finished);

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(FormatTask, "FormatA", 4096, &context, tskIDLE_PRIORITY + 5, nullptr, 0));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(FormatTask, "FormatB", 4096, &context, tskIDLE_PRIORITY + 5, nullptr, 1));
    //
    //  Give both starts above the tasks' priority.  Otherwise the first give can wake the
    //  task on this core, which then runs to the end before the second start is given, and
    //  the two never overlap.
    //
    const UBaseType_t priority = uxTaskPriorityGet(nullptr);
    vTaskPrioritySet(nullptr, tskIDLE_PRIORITY + 6);
    xSemaphoreGive(context.Start);
    xSemaphoreGive(context.Start);
    vTaskPrioritySet(nullptr, priority);
    for (uint32_t task = 0; task < 2; task++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(context.Finished, FORMAT_TIMEOUT / portTICK_PERIOD_MS), "A formatting task did not finish.");
    }

    printf("%" PRIu32 " wrong addresses in %" PRIu32 ".\n", context.WrongResults.load(), static_cast<uint32_t>(2 * FORMAT_ITERATIONS));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, context.WrongResults.load(), "A task was given another task's address.");

    vSemaphoreDelete(context.Start);
    vSemaphoreDelete(context.Finished);
}
