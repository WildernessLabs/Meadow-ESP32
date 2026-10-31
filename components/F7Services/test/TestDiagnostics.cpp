/*
 *  TestDiagnostics.cpp
 *
 *  Tests that the FreeRTOS run-time statistics count CPU time, and that the diagnostics report
 *  the CPU time each task has used since the last report rather than since boot.
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"

#include "Diagnostics.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Diagnostics][Emulated]"

/**
 * @brief Class of the tests in this file that need a board.
 *
 * Under QEMU the run-time counters stay at zero even with the statistics turned on, so the
 * busy task test can only show anything on hardware.
 */
#define CLASS_OF_HARDWARE_TESTS "[Diagnostics][HardwareOnly]"

/**
 * @brief Time the busy task spends running, in microseconds.
 */
#define BUSY_TIME_US 200000

/**
 * @brief Least run time the busy task must be credited with, in microseconds.
 *
 * Less than BUSY_TIME_US, because a higher priority task or an interrupt may take some of the
 * time the busy task is waiting out.
 */
#define MINIMUM_BUSY_RUN_TIME_US 100000

/**
 * @brief Gives the tests access to the private members of Diagnostics.
 */
class TestDiagnostics
{
public:
    /**
     * @brief Forget every remembered run time, as at boot.
     */
    static void Reset()
    {
        Diagnostics::_numberOfPreviousRunTimes = 0;
        Diagnostics::_previousTotalRunTime = 0;
    }

    /**
     * @brief Remember the run times of the tasks given.
     */
    static void Remember(const TaskStatus_t *tasks, const UBaseType_t numberOfTasks, const configRUN_TIME_COUNTER_TYPE totalRunTime)
    {
        Diagnostics::RememberRunTimes(tasks, numberOfTasks, totalRunTime);
    }

    /**
     * @brief Run time a task has used since the run times were last remembered.
     */
    static configRUN_TIME_COUNTER_TYPE SinceLastReport(const UBaseType_t taskNumber, const configRUN_TIME_COUNTER_TYPE runTime)
    {
        return (Diagnostics::RunTimeSinceLastReport(taskNumber, runTime));
    }

    /**
     * @brief Percentage of one core that a run time represents over an interval.
     */
    static uint32_t Percentage(const configRUN_TIME_COUNTER_TYPE runTime, const configRUN_TIME_COUNTER_TYPE interval)
    {
        return (Diagnostics::PercentageOfInterval(runTime, interval));
    }

    /**
     * @brief Total run time remembered at the last report.
     */
    static configRUN_TIME_COUNTER_TYPE PreviousTotal()
    {
        return (Diagnostics::_previousTotalRunTime);
    }

    /**
     * @brief Number of tasks whose run time is remembered.
     */
    static size_t MaximumTrackedTasks()
    {
        return (Diagnostics::MAXIMUM_TRACKED_TASKS);
    }
};

/**
 * @brief Make a task state with the number and run time given, and everything else zero.
 *
 * @param taskNumber FreeRTOS task number.
 * @param runTime Run-time counter.
 * @return The task state.
 */
static TaskStatus_t MakeTaskStatus(const UBaseType_t taskNumber, const configRUN_TIME_COUNTER_TYPE runTime)
{
    TaskStatus_t status;
    memset(&status, 0, sizeof(status));
    status.xTaskNumber = taskNumber;
    status.ulRunTimeCounter = runTime;
    return (status);
}

/**
 * @brief Semaphore the busy task gives when it has finished.
 */
static SemaphoreHandle_t _busyTaskFinished = nullptr;

/**
 * @brief Task that keeps the CPU busy for BUSY_TIME_US and then waits to be deleted.
 *
 * @param parameters Not used.
 */
static void BusyTask(void *parameters)
{
    const int64_t end = esp_timer_get_time() + BUSY_TIME_US;
    while (esp_timer_get_time() < end)
    {
    }
    xSemaphoreGive(_busyTaskFinished);
    vTaskSuspend(nullptr);
}

TEST_CASE("Run-time statistics count the CPU time a busy task uses", CLASS_OF_HARDWARE_TESTS)
{
    _busyTaskFinished = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(_busyTaskFinished);

    TaskHandle_t busyTask = nullptr;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(BusyTask, "BusyTask", 2048, nullptr, 1, &busyTask, 1));
    TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(_busyTaskFinished, pdMS_TO_TICKS(5000)), "The busy task did not finish.");

    TaskStatus_t status;
    vTaskGetInfo(busyTask, &status, pdFALSE, eInvalid);
    vTaskDelete(busyTask);
    vSemaphoreDelete(_busyTaskFinished);
    _busyTaskFinished = nullptr;

    char message[80];
    snprintf(message, sizeof(message), "The busy task was credited with %" PRIu32 " us of run time.", static_cast<uint32_t>(status.ulRunTimeCounter));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(MINIMUM_BUSY_RUN_TIME_US, static_cast<uint32_t>(status.ulRunTimeCounter), message);
}

TEST_CASE("The run time reported is the run time used since the last report", CLASS_OF_TESTS)
{
    TestDiagnostics::Reset();
    const TaskStatus_t tasks[] = {MakeTaskStatus(3, 1000), MakeTaskStatus(7, 250000)};
    TestDiagnostics::Remember(tasks, 2, 500000);
    TEST_ASSERT_EQUAL_UINT32(500000, TestDiagnostics::PreviousTotal());

    TEST_ASSERT_EQUAL_UINT32(4000, TestDiagnostics::SinceLastReport(3, 5000));
    TEST_ASSERT_EQUAL_UINT32(0, TestDiagnostics::SinceLastReport(7, 250000));
    TestDiagnostics::Reset();
}

TEST_CASE("A task not in the last report is credited with all of its run time", CLASS_OF_TESTS)
{
    TestDiagnostics::Reset();
    const TaskStatus_t tasks[] = {MakeTaskStatus(3, 1000)};
    TestDiagnostics::Remember(tasks, 1, 2000);

    TEST_ASSERT_EQUAL_UINT32(12345, TestDiagnostics::SinceLastReport(9, 12345));
    TestDiagnostics::Reset();
}

TEST_CASE("The run time reported is right when the counter wraps between reports", CLASS_OF_TESTS)
{
    TestDiagnostics::Reset();
    const configRUN_TIME_COUNTER_TYPE beforeWrap = static_cast<configRUN_TIME_COUNTER_TYPE>(0) - 0x100;
    const TaskStatus_t tasks[] = {MakeTaskStatus(4, beforeWrap)};
    TestDiagnostics::Remember(tasks, 1, beforeWrap);

    TEST_ASSERT_EQUAL_UINT32(0x200, TestDiagnostics::SinceLastReport(4, 0x100));
    TestDiagnostics::Reset();
}

TEST_CASE("Tasks deleted since the last report are forgotten", CLASS_OF_TESTS)
{
    TestDiagnostics::Reset();
    const TaskStatus_t first[] = {MakeTaskStatus(3, 1000), MakeTaskStatus(5, 2000)};
    TestDiagnostics::Remember(first, 2, 3000);
    const TaskStatus_t second[] = {MakeTaskStatus(3, 1500)};
    TestDiagnostics::Remember(second, 1, 4000);

    //
    //  Task 5 is not in the second report, so a task numbered 5 is treated as new.
    //
    TEST_ASSERT_EQUAL_UINT32(2500, TestDiagnostics::SinceLastReport(5, 2500));
    TEST_ASSERT_EQUAL_UINT32(100, TestDiagnostics::SinceLastReport(3, 1600));
    TestDiagnostics::Reset();
}

TEST_CASE("Run times are remembered for no more tasks than there is room for", CLASS_OF_TESTS)
{
    TestDiagnostics::Reset();
    const size_t numberOfTasks = TestDiagnostics::MaximumTrackedTasks() + 2;
    TaskStatus_t *tasks = new TaskStatus_t[numberOfTasks];
    for (size_t index = 0; index < numberOfTasks; index++)
    {
        tasks[index] = MakeTaskStatus(static_cast<UBaseType_t>(index + 1), 1000);
    }
    TestDiagnostics::Remember(tasks, static_cast<UBaseType_t>(numberOfTasks), 5000);
    delete[] tasks;

    //
    //  The last task that fits is remembered, the one after it is not.
    //
    const UBaseType_t lastRemembered = static_cast<UBaseType_t>(TestDiagnostics::MaximumTrackedTasks());
    TEST_ASSERT_EQUAL_UINT32(500, TestDiagnostics::SinceLastReport(lastRemembered, 1500));
    TEST_ASSERT_EQUAL_UINT32(1500, TestDiagnostics::SinceLastReport(lastRemembered + 1, 1500));
    TestDiagnostics::Reset();
}

TEST_CASE("The CPU percentage is the share of the interval and does not overflow", CLASS_OF_TESTS)
{
    TEST_ASSERT_EQUAL_UINT32(50, TestDiagnostics::Percentage(500, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, TestDiagnostics::Percentage(500, 0));
    TEST_ASSERT_EQUAL_UINT32(100, TestDiagnostics::Percentage(0xf0000000, 0xf0000000));
    TEST_ASSERT_EQUAL_UINT32(1, TestDiagnostics::Percentage(0x03000000, 0xf0000000));
}
