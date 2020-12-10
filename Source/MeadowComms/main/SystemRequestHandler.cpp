/*
 *  SystemRequestHandler.cpp
 *
 *  Implementation of the methods to provide the System Request Handler functionality.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>

#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "MessageDispatcher.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "DebugSettings.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
/**
 *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool SystemRequestHandler::_instantiated = false;

/**
 *  @brief Default value for _maximumMessageQueueLength.
 */
uint32_t SystemRequestHandler::_maximumMessageQueueLength = 10;

/**
 *  @brief Name of the NVS namespace holding the configuration values.
 */
const char *SystemRequestHandler::_configurationName = "Meadow";

/**
 *  @brief Name of the key holding the _maximumMessageQueueLength value.
 */
const char *SystemRequestHandler::MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME = "MaxQueueLen";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the system request handler class.
 */
SystemRequestHandler::SystemRequestHandler()
{
    if (_instantiated)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instantiated = true;
    }
}

/**
 *  @brief Initialise a new System Request handler object.
 *
 *  @param messageDispatcher
 *      Message dispatcher object that will deal with and communication between the ESP32 and the STM32.
 */
SystemRequestHandler::SystemRequestHandler(MessageDispatcher *messageDispatcher) : SystemRequestHandler()
{
    //
    //  Get configuration from NVS.
    //
    _maximumMessageQueueLength = GetConfigurationValue(MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME, _maximumMessageQueueLength);
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Default destructor for the system request handler class.
 */
SystemRequestHandler::~SystemRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Getters and setters.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the maximum length of a message queue.
 * 
 *  @return
 *      Current value for the maximum length of the message queues.
 */
uint32_t SystemRequestHandler::GetMaximumMessageQueueLength()
{
    return(_maximumMessageQueueLength);
}

/**
 *  @brief Set the maximum length of the message queues.
 * 
 *  Note that changes to this value will not take place until the next
 *  time the board is restarted.
 * 
 *  @param value
 *      New length for the message queues.
 */
StatusCodes::StatusCodes SystemRequestHandler::SetMaximumMessageQueueLength(uint32_t value)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (_maximumMessageQueueLength != value)
    {
        if (SetConfigurationValue(MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _maximumMessageQueueLength = value;
        }
    }
    return(result);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the system request handler.
 */
void SystemRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, _taskName, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  @brief Get a string configuration value.
 * 
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 * 
 *  @param name
 *      Name of the configuration value to read from NVS.
 * 
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 * 
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
char *SystemRequestHandler::GetConfigurationValue(const char *name, const char *defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a string configuration value.
 * 
 *  Store a string value in NVS
 * 
 *  @param name
 *      Name of the configuration value to set in NVS.
 * 
 *  @param value
 *      Value to be stored.
 * 
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const char *value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get an unsigned integer configuration value.
 * 
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 * 
 *  @param name
 *      Name of the configuration value to read from NVS.
 * 
 *  @param value
 *      Default value to be used if the value is not found.
 * 
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint32_t SystemRequestHandler::GetConfigurationValue(const char *name, const uint32_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set an unsigned integer configuration value.
 * 
 *  Set an unsigned integer value in NVS
 * 
 *  @param name
 *      Name of the configuration value to set in NVS.
 * 
 *  @param value
 *      Value to be set.
 * 
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const uint32_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get an integer configuration value.
 * 
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 * 
 *  @param name
 *      Name of the configuration value to read from NVS.
 * 
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 * 
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
int SystemRequestHandler::GetConfigurationValue(const char *name, const int32_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set an integer configuration value.
 * 
 *  Set an integer value in NVS
 * 
 *  @param name
 *      Name of the configuration value to set in NVS.
 * 
 *  @param value
 *      Value to be set.
 * 
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const int32_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get a byte configuration value.
 * 
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 * 
 *  @param name
 *      Name of the configuration value to read from NVS.
 * 
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 * 
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint8_t SystemRequestHandler::GetConfigurationValue(const char *name, const uint8_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a byte configuration value.
 * 
 *  Set a byte value in NVS
 * 
 *  @param name
 *      Name of the configuration value to set in NVS.
 * 
 *  @param value
 *      Value to be set.
 * 
 *  @returns
 *      Error code indicating success or failure status.
*/
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const uint8_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get a boolean configuration value.
 * 
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 * 
 *  @param name
 *      Name of the configuration value to read from NVS.
 * 
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 * 
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
bool SystemRequestHandler::GetConfigurationValue(const char *name, const bool defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a boolean configuration value.
 * 
 *  Set a boolean value in NVS
 * 
 *  @param name
 *      Name of the configuration value to set in NVS.
 * 
 *  @param value
 *      Value to be set.
 * 
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const bool value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get the configuration for the system.
 *
 *  @param request
 *      Message requesting the system configuration.
 */
void SystemRequestHandler::GetConfiguration(Message *request)
{
#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter")
#endif

    Esp32Messaging::SystemConfiguration config = {};
    const esp_app_desc_t *applicationDescription = esp_ota_get_app_description();
    config.SoftwareVersion = const_cast<char *>(applicationDescription->version);
    config.Antenna = WiFiRequestHandler::GetAntenna();
    config.MaximumRetryCount = WiFiRequestHandler::GetMaximumRetryCount();
    config.AutomaticallyReconnect = WiFiRequestHandler::GetAutomaticReconnect();
    config.AutomaticallyStartNetwork = WiFiRequestHandler::GetAutomaticallyStartNetwork();
    config.GetTimeAtStartup = WiFiRequestHandler::GetGetTimeAtStartup();
    config.UseDhcp = WiFiRequestHandler::GetUseDhcp();
    config.NtpServer = WiFiRequestHandler::GetNtpServer();
    config.DefaultAccessPoint = WiFiRequestHandler::GetDefaultAccessPoint();
    config.StaticIpAddress = WiFiRequestHandler::GetIpAddress();
    config.DnsServer = WiFiRequestHandler::GetDnsServer();
    config.DefaultGateway = WiFiRequestHandler::GetDefaultGateway();
    config.DeviceName = nullptr;
    WiFiRequestHandler::GetMacAddress(config.BoardMacAddress, WIFI_IF_STA);
    WiFiRequestHandler::GetMacAddress(config.SoftApMacAddress, WIFI_IF_AP);

    request->PayloadLength = Encoders::EncodedSystemConfigurationBufferSize(&config);
    request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
    Encoders::EncodeSystemConfiguration(&config, request->Payload);
    request->MessageType = MessageTypes::Interrupt;
    request->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(request);

#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit")
#endif
}

/**
 *  @brief Set a single configuration value.
 *
 *  @param request
 *      Message requesting the system configuration.
 */
void SystemRequestHandler::SetConfiguration(Message *request)
{
#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter")
#endif

    Esp32Messaging::ConfigurationValue *value = Encoders::ExtractConfigurationValue(request->Payload);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    switch (value->Item)
    {
        case ConfigurationItems::Antenna:
            WiFiRequestHandler::SetAntenna(*value->Value);
            break;
        case ConfigurationItems::AutomaticallyReconnect:
            result = WiFiRequestHandler::SetAutomaticReconnect((uint8_t) (*value->Value) == 1);
            break;
        case ConfigurationItems::AutomaticallyStartNetwork:
            result = WiFiRequestHandler::SetAutomaticallyStartNetwork((uint8_t) (*value->Value) == 1);
            break;
        case ConfigurationItems::BoardMacAddress:
            result = WiFiRequestHandler::SetMacAddress(value->Value, WIFI_IF_STA);
            break;
        case ConfigurationItems::SoftApMacAddress:
            result = WiFiRequestHandler::SetMacAddress(value->Value, WIFI_IF_AP);
            break;
        case ConfigurationItems::DefaultApAndPassword:
            result = WiFiRequestHandler::SetDefaultAccessPoint((char *) value->Value);
            if (result == StatusCodes::CompletedOk)
            {
                result = WiFiRequestHandler::SetPassword((char *) (value->Value + strlen((char *) value->Value) + 1));
            }
            break;
        case ConfigurationItems::DeviceName:
            result = WiFiRequestHandler::SetDeviceName((char *) value->Value);
            break;
        case ConfigurationItems::DnsServer:
            result = WiFiRequestHandler::SetDnsServer((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::StaticIpAddress:
            result = WiFiRequestHandler::SetIpAddress((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::DefaultGateway:
            result = WiFiRequestHandler::SetDefaultGateway((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::GetTimeAtStartup:
            result = WiFiRequestHandler::SetGetTimeAtStartup((uint8_t) (*value->Value) == 1);
            break;
        case ConfigurationItems::MaximumMessageQueueLength:
            result = SetMaximumMessageQueueLength((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::MaximumRetryCount:
            result = WiFiRequestHandler::SetMaximumRetryCount((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::NtpServer:
            result = WiFiRequestHandler::SetNtpServer((char *) value->Value);
            break;
        case ConfigurationItems::UseDhcp:
            result = WiFiRequestHandler::SetUseDhcp((uint8_t) (*value->Value) == 1);
            break;
    }
    request->DeletePayload();
    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(request);

#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit")
#endif
}

/**
 *  @brief Put the system into deep sleep.
 *
 *  This is one of the few messages that will not send a response to the
 *  STM32.  The ESP32 will simply go to sleep.
 *
 *  @param request
 *      Message requesting the ESP32 got to sleep.
 */
void SystemRequestHandler::DeepSleep(Message *request)
{
#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter")
#endif
    
    ESP_LOGI(__func__, "DeepSleep not implemented");

#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit")
#endif
}

/**
 *  @brief Get the voltage reading from the battery charging circuit.
 *
 *  @param request
 *      Message requesting the battery level.
 */
void SystemRequestHandler::GetBatteryChargeLevel(Message *message)
{
#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter")
#endif

    ESP_LOGI(__func__, "BatteryLevel not implemented");
    //
    //  TODO: Complete the implemention of the BatteryLevel method.
    //        For the minute we'll fake it at 2.5V.
    //
    Esp32Messaging::IntegerResponse response;
    response.Result = 2500;

    message->PayloadLength = Encoders::EncodedIntegerResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerResponse(&response, message->Payload);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit")
#endif
}

/**
 *  Dispatch the System request to the appropriate method in the class.
 *
 *  @param request
 *      Request to be processed.
 */
void SystemRequestHandler::DispatchRequest(Message *request)
{
#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter")
#endif

    switch (request->Function)
    {
        case SystemFunction::GetConfiguration:
            GetConfiguration(request);
            break;
        case SystemFunction::SetConfigurationItem:
            SetConfiguration(request);
            break;
        case SystemFunction::DeepSleep:
            break;
        case SystemFunction::GetBatteryChargeLevel:
            GetBatteryChargeLevel(request);
            break;
        default:
            ESP_LOGI(__func__, "Unknown System message received, function 0x%x", request->Function);
            if (request->Payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }

#if defined(SYSTEM_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit")
#endif
}
