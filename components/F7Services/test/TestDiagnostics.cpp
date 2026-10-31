/*
 *  TestDiagnostics.cpp
 *
 *  Tests that the FreeRTOS run-time statistics count CPU time.
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
