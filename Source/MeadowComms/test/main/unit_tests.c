/*
 *  file_system_test.c
 *
 *  Run the file system tests.
 */

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_heap_trace.h"

#include <stdio.h>
#include <string.h>
#include "unity.h"

#define NUMBER_OF_HEAP_TRACE_RECORDS 100

static void print_banner(const char *text)
{
    printf("\n#### %s #####\n\n", text);
}

/**
 *  @brief Array holding the heap trace records.
 */
heap_trace_record_t heapTraceRecords[NUMBER_OF_HEAP_TRACE_RECORDS];

void app_main(void)
{
    //
    //  Initialise the NVS storage.
    //
    esp_err_t error = nvs_flash_init();
    if ((error == ESP_ERR_NVS_NO_FREE_PAGES) || (error == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* These are the different ways of running registered tests.
     * In practice, only one of them is usually needed.
     *
     * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
     * (number of tests executed/failed/ignored) of tests executed between these calls.
     */
    // print_banner("Running tests with [FileSystem] tag");
    // UNITY_BEGIN();
    // unity_run_tests_by_tag("[FileSystem]", false);
    // UNITY_END();

    // print_banner("Running all the registered tests");
    // UNITY_BEGIN();
    // unity_run_all_tests();
    // UNITY_END();

    print_banner("Starting interactive test menu");
    /* This function will not return, and will be busy waiting for UART input.
     * Make sure that task watchdog is disabled if you use this function.
     */
    unity_run_menu();

    vTaskSuspend(NULL);
}