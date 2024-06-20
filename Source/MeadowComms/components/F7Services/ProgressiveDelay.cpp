/*
 *  ProgressiveDelay.cpp
 *
 *  Implementation of the methods to provide the Progressive Delay functionality.
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
 * @brief Delay periods in milliseconds)
 */
// ProgressiveDelay::_delayPeriods{ 100, 300, 700, 1000, 5000, 10000, 20000 };

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
 * @brief Restart the Progressive Delay
 */
void ProgressiveDelay::Restart()
{
    _currentPeriod = 0;
}

/**
 * @brief Delay for the current period and move the index onto the next period.
 */
void ProgressiveDelay::Delay()
{
    vTaskDelay(_delayPeriods[_currentPeriod] / portTICK_PERIOD_MS);
    if (_currentPeriod < ((sizeof(_delayPeriods) / sizeof(_delayPeriods[0])) - 1))
    {
        _currentPeriod++;
    }
}
