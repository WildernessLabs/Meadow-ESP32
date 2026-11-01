/*
 *  TestDiagnostics.cpp
 *
 *  Tests that the FreeRTOS run-time statistics count CPU time, that the diagnostics report the CPU
 *  time each task has used since the last report rather than since boot, and that a snapshot writes
 *  its records.
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"

#include "Diagnostics.hpp"
#include "Metrics.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Diagnostics][Emulated]"

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

TEST_CASE("Run-time statistics count the CPU time a busy task uses", CLASS_OF_TESTS)
{
    _busyTaskFinished = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(_busyTaskFinished);

    TaskHandle_t busyTask = nullptr;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(BusyTask, "BusyTask", 2048, nullptr, 1, &busyTask, 1));
    TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(_busyTaskFinished, pdMS_TO_TICKS(5000)), "The busy task did not finish.");

    //
    //  A task's run time is only added to its counter when it is switched out, and nothing
    //  switches the busy task out until it suspends itself.  Reading the counter as soon as
    //  the semaphore arrives can catch it still running with nothing yet counted.
    //
    for (int attempt = 0; (attempt < 100) && (eTaskGetState(busyTask) != eSuspended); attempt++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_EQUAL_MESSAGE(eSuspended, eTaskGetState(busyTask), "The busy task did not suspend itself.");

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

/**
 * @brief Size of the buffer the log output is captured in.
 */
#define CAPTURE_SIZE 16384

/**
 * @brief Number of snapshots taken by the leak test.
 */
#define LEAK_TEST_SNAPSHOTS 20

/**
 * @brief Bytes the free heap may fall by over the leak test, for bookkeeping that is not a leak.
 */
#define HEAP_ALLOWANCE 512

/**
 * @brief Log output captured while a snapshot is taken.
 */
static char _captured[CAPTURE_SIZE];

/**
 * @brief Number of characters in _captured.
 */
static size_t _capturedLength = 0;

/**
 * @brief Log output function that appends to _captured, dropping what does not fit.
 *
 * @param format Format of the message.
 * @param arguments Values for the format.
 * @return Number of characters the message came to.
 */
static int CaptureLogOutput(const char *format, va_list arguments)
{
    const size_t space = sizeof(_captured) - _capturedLength;
    const int length = vsnprintf(_captured + _capturedLength, space, format, arguments);
    if (length > 0)
    {
        _capturedLength += ((size_t) length < space) ? (size_t) length : (space - 1);
    }
    return (length);
}

/**
 * @brief Take a snapshot with the log output captured.
 *
 * @return Sequence number of the snapshot.
 */
static uint32_t CaptureSnapshot()
{
    _capturedLength = 0;
    _captured[0] = 0;
    const vprintf_like_t previous = esp_log_set_vprintf(CaptureLogOutput);
    const uint32_t sequence = Diagnostics::TakeSnapshot();
    esp_log_set_vprintf(previous);
    return (sequence);
}

/**
 * @brief Find a record of the snapshot that starts with the text given.
 *
 * @param start Start of the record after @diag, for example "heap,3,internal,".
 * @param record Set to the record, up to the end of its line, when it is found.
 * @param size Size of record.
 * @return true if the record was found.
 */
static bool FindRecord(const char *start, char *record, const size_t size)
{
    char wanted[64];
    snprintf(wanted, sizeof(wanted), "@diag,%s", start);
    const char *found = strstr(_captured, wanted);
    if (found == nullptr)
    {
        return (false);
    }
    size_t length = strcspn(found, "\r\n\033");
    length = (length < (size - 1)) ? length : (size - 1);
    memcpy(record, found, length);
    record[length] = 0;
    return (true);
}

/**
 * @brief Count the comma separated fields in a record, @diag included.
 *
 * @param record Record.
 * @return Number of fields.
 */
static uint32_t CountFields(const char *record)
{
    uint32_t fields = 1;
    for (const char *character = record; *character != 0; character++)
    {
        if (*character == ',')
        {
            fields++;
        }
    }
    return (fields);
}

TEST_CASE("A snapshot writes each kind of record with its sequence number", CLASS_OF_TESTS)
{
    //
    //  Logging for the component is off, as it is at boot; the snapshot turns it on.
    //
    esp_log_level_set(Diagnostics::COMPONENT_NAME, ESP_LOG_NONE);
    const uint32_t sequence = CaptureSnapshot();

    const char *starts[] = {
        "snapshot,%lu,",
        "task,%lu,",
        "cpu,%lu,",
        "heap,%lu,internal,",
        "heap,%lu,dma,",
#if defined(CONFIG_MEADOW_METRICS)
        "counter,%lu,RequestsReceived,",
        "gauge,%lu,OutboundQueueDepth,",
        "histogram,%lu,WiFiRequestLatency,",
#endif
#if defined(CONFIG_LWIP_STATS)
        "network,%lu,tcp,",
#endif
        "end,%lu,"};
    for (const char *start: starts)
    {
        char wanted[48];
        snprintf(wanted, sizeof(wanted), start, (unsigned long) sequence);
        char record[Diagnostics::RECORD_SIZE];
        char message[96];
        snprintf(message, sizeof(message), "No record starting @diag,%s was written.", wanted);
        TEST_ASSERT_TRUE_MESSAGE(FindRecord(wanted, record, sizeof(record)), message);
    }
}

TEST_CASE("Every record of a snapshot carries its sequence number", CLASS_OF_TESTS)
{
    const uint32_t sequence = CaptureSnapshot();
    char expected[16];
    snprintf(expected, sizeof(expected), ",%lu", (unsigned long) sequence);

    uint32_t records = 0;
    for (const char *found = strstr(_captured, "@diag,"); found != nullptr; found = strstr(found + 1, "@diag,"))
    {
        const char *type = found + strlen("@diag,");
        const char *sequenceField = strchr(type, ',');
        TEST_ASSERT_NOT_NULL(sequenceField);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, strncmp(sequenceField, expected, strlen(expected)), "A record has another snapshot's sequence number.");
        records++;
    }
    TEST_ASSERT_GREATER_THAN_UINT32(10, records);
}

TEST_CASE("Each snapshot has the next sequence number", CLASS_OF_TESTS)
{
    const uint32_t first = CaptureSnapshot();
    const uint32_t second = CaptureSnapshot();
    TEST_ASSERT_EQUAL_UINT32(first + 1, second);
    TEST_ASSERT_EQUAL_UINT32(second, Diagnostics::LastSequence());
}

TEST_CASE("A snapshot lists the task that took it", CLASS_OF_TESTS)
{
    const uint32_t sequence = CaptureSnapshot();
    char wanted[48];
    snprintf(wanted, sizeof(wanted), ",%s,", pcTaskGetName(nullptr));
    char prefix[24];
    snprintf(prefix, sizeof(prefix), "@diag,task,%lu,", (unsigned long) sequence);

    bool found = false;
    for (const char *record = strstr(_captured, prefix); (record != nullptr) && !found; record = strstr(record + 1, prefix))
    {
        const size_t length = strcspn(record, "\r\n\033");
        const char *name = strstr(record, wanted);
        found = (name != nullptr) && (name < (record + length));
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "The task taking the snapshot is not in it.");
}

#if defined(CONFIG_MEADOW_METRICS)

TEST_CASE("A histogram record holds a count for every bucket", CLASS_OF_TESTS)
{
    const uint32_t sequence = CaptureSnapshot();
    char wanted[48];
    snprintf(wanted, sizeof(wanted), "histogram,%lu,SystemHandlerTime,", (unsigned long) sequence);
    char record[Diagnostics::RECORD_SIZE];
    TEST_ASSERT_TRUE(FindRecord(wanted, record, sizeof(record)));
    //
    //  @diag, histogram, sequence, name, count, total, longest, then the buckets.
    //
    TEST_ASSERT_EQUAL_UINT32(7 + Metrics::NUMBER_OF_BUCKETS, CountFields(record));
}

#endif

TEST_CASE("Taking snapshots does not leak memory", CLASS_OF_TESTS)
{
    //
    //  The first snapshot makes the snapshot mutex, which is kept, so it is taken before the
    //  free heap is measured.
    //
    CaptureSnapshot();
    const size_t before = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    for (uint32_t index = 0; index < LEAK_TEST_SNAPSHOTS; index++)
    {
        CaptureSnapshot();
    }
    const size_t after = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(HEAP_ALLOWANCE, (before > after) ? (before - after) : 0, "Taking snapshots lost memory.");
}

TEST_CASE("A requested snapshot is taken by the snapshot task", CLASS_OF_TESTS)
{
    //
    //  With a period of 0 the task takes a snapshot only when asked.  The task is kept for the
    //  rest of the run, as it would be in the application.
    //
    Diagnostics::Setup(0);
    const uint32_t before = Diagnostics::LastSequence();
    const vprintf_like_t previous = esp_log_set_vprintf(CaptureLogOutput);
    _capturedLength = 0;
    Diagnostics::RequestSnapshot();
    for (int attempt = 0; (attempt < 200) && (Diagnostics::LastSequence() == before); attempt++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    esp_log_set_vprintf(previous);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before + 1, Diagnostics::LastSequence(), "The snapshot task did not take a snapshot.");
}
