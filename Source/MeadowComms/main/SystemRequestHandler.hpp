/*
 *  SystemRequestHandler.hpp
 *
 *  System message request handler
 */

#ifndef _SYSTEM_REQUEST_HANDLER_HPP_
#define _SYSTEM_REQUEST_HANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <string.h>
#include <stdlib.h>

#include "RequestHandlerBase.hpp"
#include "NvsManager.hpp"

/**
 *  @brief Object to deal with system requests such as configuration settings etc.
 */
class SystemRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  Name of the FreeRTOS task for the System task.
     */
    const char *_taskName = "SystemTask";

    /**
     *  Name of the NVS store holding the system configuration.
     */
    static const char *_configurationName;

    /**
     *  Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     *  Maximum length of a message queue.
     */
    static uint32_t _maximumMessageQueueLength;

    /**
     *  Name of the NVS storage holding the _maximumMessagequeueLength value.
     */
    static const char *MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME;

    /**
     *  This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /**
     *  Constructor(s) and destructor.
     */
    SystemRequestHandler();
    ~SystemRequestHandler();

    /**
     *  Get the system configuration.
     */
    void GetConfiguration(Message *);

    /**
     *  Set the system configuration.
     */
    void SetConfiguration(Message *);

    /**
     *  Put the ESP32 into deep sleep.
     */
    void DeepSleep(Message *);

    /**
     *  Get the battery level.
     */
    void GetBatteryChargeLevel(Message *);

public:
    /**
     *  Public constructor.
     */
    explicit SystemRequestHandler(MessageDispatcher *);

    /**
     *  Public methods.
     */
    void Setup();

    /**
     *  Get a string configuration value.
     */
    static char *GetConfigurationValue(const char *, const char *);

    /**
     *  Get an unsigned integer configuration value.
     */
    static uint32_t GetConfigurationValue(const char *, const uint32_t);

    /**
     *  Get an integer configuration value.
     */
    static int GetConfigurationValue(const char *, const int32_t);

    /**
     *  Get a byte configuration value.
     */
    static uint8_t GetConfigurationValue(const char *, const uint8_t);

    /**
     *  Get a boolean configuration value.
     */
    static bool GetConfigurationValue(const char *, const bool);

    /**
     *  Get / Set the maximum message queue length.
     */
    static uint32_t GetMaximumMessageQueueLength();
    static StatusCodes::StatusCodes SetMaximumMessageQueueLength(uint32_t);

    /**
     *  Set a string configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const char *);

    /**
     *  Set an unsigned integer configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const uint32_t);

    /**
     *  Set an integer configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const int32_t);

    /**
     *  Set a byte configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const uint8_t);

    /**
     *  Set a boolean configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const bool);
};

#endif /* _SYSTEM_REQUEST_HANDLER_HPP_ */
