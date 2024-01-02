/**
 * 
 * Timer.hpp
 * 
 * Hardware timer to general-purpose
*/
#ifndef _TIMER_GROUP_HPP_
#define _TIMER_GROUP_HPP_

#include "sdkconfig.h"
#include <string>
#include <string.h>
#include <functional>

#include "esp_timer.h"

#include "SharedEnums.hpp"

/**
 *  @brief TimerHighResolution class
 *
 *  Encapsulate the properties and methods required to provide the
 *  ability to manipulate the Timer.
 */
class TimerHighResolution
{
private:

    /**
     * @brief Alias to indicate the function type
    */
    using TimerCallback = void(*)(void*);

    /**
     * @brief Namer of the timer.
    */
    std::string _nameTimer;

    /**
     * @brief Handler to hold the created timer.
    */
    esp_timer_handle_t _timer_handler = nullptr;

    /**
    * @brief Period of the timer.
    */
    uint64_t _timerPeriod;

public:

    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new High Resolution Timer (ESP TIMER) object
     * 
     * @param timerCallback
     *      Function to call when timer expires.
     * @param _nameTimer
     *      Timer namer
     */
    TimerHighResolution(TimerCallback timerCallback, std::string _nameTimer);

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
    StatusCodes::StatusCodes Start(bool periodic);

    /**
     * @brief Stop the timer.
     *  
     * @return
     *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
     */
    StatusCodes::StatusCodes Stop();

    /**
     * @brief Set the period of the timer.
     * 
     * @param value 
     *      value in milliseconds of the period
     */
    void SetPeriod(uint64_t value);

    /**
     * @brief Delete the timer.
     * 
     * @return
     *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
     */
    StatusCodes::StatusCodes Delete();

};

#endif  /* _TIMER_GROUP_HPP_ */
