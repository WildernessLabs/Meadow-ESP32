/**
 * @file Globals.hpp
 * @author Mark Stevens
 * @brief Provide a home for global variables.
 * @date 2024-12-12
 *
 * @copyright Copyright (c) 2024 Wilderness Labs
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************************
 *
 *  Task priorities.
 *
 *  The task priorities on the ESP32 are defined as a number between 0 and 24 (inclusive).
 *
 *  Priority 0 is the lowest priority and is used by the idle task.  We should place our
 *  tasks at a higher priority than the idle task but also order them to ensure that the
 *  most commonly used functionality has the highest priority without interfering with the
 *  operating system.  We also need to ensure that the inter-processor communication tasks
 *  are given a high priority to ensure that the system remains responsive.
 *
 *  The priorities below are placed in order from highest to lowest.
 */

/**
 * @brief The SPI task is used to process messages from the STM32.
 *
 * @note This task is the highest priority task as it is the most time critical task and it
 *       is driven by the STM32 and so we have little control over the timing of the messages.
 */
#define PRIORITY_SPI_TASK (16)

/**
 * @brief The WiFi task is used to process WiFi requests from the STM32.  This is the most
 *        common use case for the system and so it is given a high priority.
 */
#define PRIORITY_WIFI_TASK (14)

/**
 * @brief This is the default priority for tasks delegated to the thread pool.  This can be
 *        any task where tasks are allowed to run in parallel.
 */
#define PRIORITY_THREAD_POOL_TASK (12)

/**
 * @brief Bluetooth tasks are not a common use case for intensive work and so have a moderate
 *        priority.
 */
#define PRIORITY_BLUETOOTH_TASK (10)

/**
 * @brief Process requests from the System requests (get battery level, configuration
 *        changes, etc).
 */
#define PRIORITY_SYSTEM_TASK (8)

/**
 * @brief The mesh network is not really implemented at the moment.
 */
#define PRIORITY_MESH_NETWORK_TASK (8)

/**
 * @brief The SPI synchronisation task is only used at system start up to start the
 *        communication between the ESP3 and STM32.
 */
#define PRIORITY_SPI_SYNCHRONISATION_TASK (8)

/**
 *  Finally, lets make sure that the priorities are valid.
 */
static_assert(PRIORITY_WIFI_TASK > 0, "WiFi priority must be greater than 0");
static_assert(PRIORITY_WIFI_TASK < configMAX_PRIORITIES, "WiFi priority must be less than the maximum priority");
static_assert(PRIORITY_SYSTEM_TASK > 0, "System priority must be greater than 0");
static_assert(PRIORITY_SYSTEM_TASK < configMAX_PRIORITIES, "System priority must be less than the maximum priority");
static_assert(PRIORITY_MESH_NETWORK_TASK > 0, "Mesh network priority must be greater than 0");
static_assert(PRIORITY_MESH_NETWORK_TASK < configMAX_PRIORITIES, "Mesh network priority must be less than the maximum priority");
static_assert(PRIORITY_THREAD_POOL_TASK > 0, "Thread pool priority must be greater than 0");
static_assert(PRIORITY_THREAD_POOL_TASK < configMAX_PRIORITIES, "Thread pool priority must be less than the maximum priority");
static_assert(PRIORITY_SPI_TASK > 0, "SPI priority must be greater than 0");
static_assert(PRIORITY_SPI_TASK < configMAX_PRIORITIES, "SPI priority must be less than the maximum priority");
static_assert(PRIORITY_SPI_SYNCHRONISATION_TASK > 0, "SPI synchronisation priority must be greater than 0");
static_assert(PRIORITY_SPI_SYNCHRONISATION_TASK < configMAX_PRIORITIES, "SPI synchronisation priority must be less than the maximum priority");
static_assert(PRIORITY_BLUETOOTH_TASK > 0, "Bluetooth priority must be greater than 0");
static_assert(PRIORITY_BLUETOOTH_TASK < configMAX_PRIORITIES, "Bluetooth priority must be less than the maximum priority");