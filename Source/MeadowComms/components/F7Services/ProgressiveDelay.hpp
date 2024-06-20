/*
 *  ProgressiveDelay.hpp
 *
 *  Class definition for the Progressive Delay class.
 */
#ifndef __PROGRESSIVE_DELAY_HPP__
#define __PROGRESSIVE_DELAY_HPP__

#include "sdkconfig.h"

#include <stdint.h>

/**
 * @brief Provide a mechanism for implementing a progressive delay for retry events.
 */
class ProgressiveDelay
{
public:
    /**
     *  @brief Name of the component / task for the WiFi system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new Progressive Delay object
     */
    ProgressiveDelay();

    /**
     * @brief Destroy the Progressive Delay object
     */
    ~ProgressiveDelay();

    /**
     * @brief Restart the Progressive Delay
     */
    void Restart();

    /**
     * @brief Delay for the current period and move the index onto the next period.
     */
    void Delay();

    /**
     * @brief Get the Current Delay Period object
     *
     * @return uint32_t Delay in milliseconds.
     */
    uint32_t GetCurrentDelayPeriod()
    {
        return _delayPeriods[_currentPeriod];
    }

private:
    /**
     * @brief Index of the next period to be used.
     */
    int _currentPeriod = 0;

    /**
     * @brief Delays in milliseconds.
     */
    uint32_t _delayPeriods[7] = {100, 300, 700, 1000, 5000, 10000, 20000};
};

#endif // __PROGRESSIVE_DELAY_HPP__