/**
 * @file ProgressiveDelay.cpp
 * @author Mark Stevens
 * @brief Implementation of progressive delay functionality.
 * @date 2024-06-20
 *
 * @copyright Copyright (c) 2024 Wilderness Labs
 */

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ProgressiveDelay.hpp"

/**
 * @brief Name of this component.
 */
const char *ProgressiveDelay::COMPONENT_NAME = "ProgressiveDelay";

/**
 * @brief Delays in milliseconds.
 */
uint32_t ProgressiveDelay::_delayPeriods[] = {100, 300, 700, 1000, 5000, 10000, 20000};

/**
 * @brief Construct a new Progressive Delay object
 */
ProgressiveDelay::ProgressiveDelay()
{
    // _delayPeriods = { 100, 300, 700, 1000, 5000, 10000, 20000 };
    Restart();
}

/**
 * @brief Destroy the Progressive Delay object
 */
ProgressiveDelay::~ProgressiveDelay()
{
}

/**
 * @brief Restart the Progressive Delay from the start of the list of delay periods.
 */
void ProgressiveDelay::Restart()
{
    _currentPeriod = 0;
}

/**
 * @brief Delay for the current period and move the index onto the next period.
 *
 * The delay will gradually get longer as the number of calls to this method until
 * we reach the end of the array of delay periods at which point the same delay
 * will be used for all subsequent calls until Restart is called.
 */
void ProgressiveDelay::Delay()
{
    vTaskDelay(_delayPeriods[_currentPeriod] / portTICK_PERIOD_MS);
    if (_currentPeriod < ((sizeof(_delayPeriods) / sizeof(_delayPeriods[0])) - 1))
    {
        _currentPeriod++;
    }
}
