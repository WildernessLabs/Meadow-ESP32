/*
 *  AccessPoint.cpp
 *
 *  Implement the access point functionality (the board acting as an access point).
 */
#include "../../build/config/sdkconfig.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lwip/apps/sntp.h"

#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Mapping.hpp"
#include "Logging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Start the access point.
 */
void WiFiRequestHandler::StartAccessPoint(Message *message)
{

}

/**
 *  @brief Stop the access point if it is running.
 */
void WiFiRequestHandler::StopAccessPoint(Message *message)
{

}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Process the node connecting to access point event.
 */
void WiFiRequestHandler::NodeConnectedEvent()
{
    TRACE_MESSAGE("NodeConnectedEvent: Enter");
    TRACE_MESSAGE("NodeConnectedEvent: Exit");
}

/**
 * @brief Processes the node disconnecting from the access point event.
 * 
 * @param eventData
 *      Information about the node that has disconnected from the access point.
 */
void WiFiRequestHandler::NodeDisconnectedEvent(wifi_event_ap_stadisconnected_t *eventData)
{
    TRACE_MESSAGE("NodeDisconnectedEvent: Enter");
    TRACE_MESSAGE("NodeDisconnectedEvent: Exit");
}
