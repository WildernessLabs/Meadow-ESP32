#include "sdkconfig.h"

#include "esp_err.h"
#include "TimerHighResolution.hpp"
#include "SharedEnums.hpp"
#include "Mapping.hpp"

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

StatusCodes::StatusCodes TimerHighResolution::Stop()
{
    esp_err_t result = ESP_OK;
    result  = esp_timer_stop(_timer_handler);
    return(Mapping::GetStatusCode(result));
}

StatusCodes::StatusCodes TimerHighResolution::Delete()
{
    esp_err_t result = ESP_OK;
    result  = esp_timer_delete(_timer_handler);
    return(Mapping::GetStatusCode(result));
}

void TimerHighResolution::SetPeriod(uint64_t value)
{
    _timerPeriod = value * 1000;
}