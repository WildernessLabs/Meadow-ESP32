/*
 *  TimerHighResolution.cpp
 *
 *  Implementation of the methods to provide the Timer High Resolution functionality.
 */

#include "sdkconfig.h"

#include "esp_err.h"
#include "TimerHighResolution.hpp"
#include "SharedEnums.hpp"
#include "Mapping.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the FreeRTOS component.
 */
const char *TimerHighResolution::COMPONENT_NAME = "TimerHighResolution";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new High Resolution Timer (ESP TIMER) object
 * 
 * @param timerCallback
 *      Function to call when timer expires.
 * @param _nameTimer
 *      Timer namer
 */

TimerHighResolution::TimerHighResolution(TimerCallback timerCallback, std::string _nameTimer)
{
    esp_timer_create_args_t _configurationTimer;
    {
        _configurationTimer.callback = timerCallback;
        _configurationTimer.arg = this;
        _configurationTimer.name = _nameTimer.c_str();
        _configurationTimer.dispatch_method = ESP_TIMER_TASK;

    }
    esp_timer_create(&_configurationTimer, &_timer_handler);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Getters and setters.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Start the timer.
 * 
 * @param periodic 
 *      true to start the timer in periodic mode
 *      false to start the timer in one-shot mode
 * 
 * @return
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes TimerHighResolution::Start(bool periodic)
{
    esp_err_t result = ESP_OK;

    if (periodic)
    {
        result = esp_timer_start_periodic(_timer_handler, _timerPeriod); 
    }
    else
    {
        result = esp_timer_start_once(_timer_handler, _timerPeriod);
    }

    return(Mapping::GetStatusCode(result));
}

/**
 * @brief Stop the timer.
 *  
 * @return
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes TimerHighResolution::Stop()
{
    esp_err_t result = ESP_OK;
    result  = esp_timer_stop(_timer_handler);
    return(Mapping::GetStatusCode(result));
}

/**
 * @brief Delete the timer.
 * 
 * @return
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes TimerHighResolution::Delete()
{
    esp_err_t result = ESP_OK;
    result  = esp_timer_delete(_timer_handler);
    return(Mapping::GetStatusCode(result));
}

/**
 * @brief Set the period of the timer.
 * 
 * @param value 
 *      value in milliseconds of the period
 */
void TimerHighResolution::SetPeriod(uint64_t value)
{
    _timerPeriod = value * 1000;
}