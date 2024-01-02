/**
 * 
 * Timer.hpp
 * 
 * Hardware timer group to general-purpose
*/
#ifndef _TIMER_GROUP_HPP_
#define _TIMER_GROUP_HPP_

#include "sdkconfig.h"
#include <string>
#include <string.h>
#include <functional>

#include "esp_timer.h"

#include "SharedEnums.hpp"

class TimerHighResolution
{
private:

    /**
     * @brief
    */
    using TimerCallback = void(*)(void*);

    /**
     * @brief
    */
    std::string _nameTimer;

    /**
     * @brief
    */
    esp_timer_handle_t _timer_handler = nullptr;

    /**
    * @brief
    */
    uint64_t _timerPeriod;

public:

    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief 
    */
    TimerHighResolution(TimerCallback timerCallback, std::string _nameTimer);

    /**
     * @brief
    */
    StatusCodes::StatusCodes Start(bool periodic);

    /**
     * @brief
    */
    StatusCodes::StatusCodes Stop();

    /**
     * @brief
    */
    void SetPeriod(uint64_t value);

    /**
     * @brief
    */
    StatusCodes::StatusCodes Delete();

};

#endif  /* _TIMER_GROUP_HPP_ */
