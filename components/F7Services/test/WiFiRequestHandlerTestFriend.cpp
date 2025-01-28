/*
 *  WiFiRequestHandlerTestFriend.cpp
 *
 *  Implementation of the test methods for the WiFiRequestHandler class.
 */

#include "sdkconfig.h"

#include "WiFiRequestHandlerTestFriend.hpp"

/**
 * @brief Construct a new WiFiRequestHandlerTestFriend object.
 *
 * @param wifiRequestHandler Instance of the WiFiRequestHandler class under test.
 */
WiFiRequestHandlerTestFriend::WiFiRequestHandlerTestFriend(WiFiRequestHandler *wifiRequestHandler) : wifiRequestHandler(wifiRequestHandler)
{
}

/**
 * @brief Setup the WiFiRequestHandler.
 */
void WiFiRequestHandlerTestFriend::Setup()
{
    wifiRequestHandler->Setup();
}

/**
 * @brief Use the WiFiRequestHandler to scan for access points.
 *
 * @param message Message containing the request for the scan for access points.
 */
void WiFiRequestHandlerTestFriend::ScanForAccessPoints(Message *message)
{
    wifiRequestHandler->ScanForAccessPoints(message);
}

/**
 * @brief Use the WiFiRequestHandler to connect to an access point.
 *
 * @param message Message containing the request to connect to an access point.
 */
void WiFiRequestHandlerTestFriend::ConnectToAccessPoint(Message *message)
{
    wifiRequestHandler->ConnectToAccessPoint(message);
}

/**
 * @brief Set the automatic reconnect flag indicating if the system should automatically
 * reconnect to the access point.
 *
 * @param value True if the system should automatically reconnect, false otherwise.
 * @return StatusCodes::StatusCodes StatusCodes::CompletedOk if the value was set, an error code otherwise.
 */
StatusCodes::StatusCodes WiFiRequestHandlerTestFriend::SetAutomaticReconnect(bool value)
{
    return (wifiRequestHandler->SetAutomaticReconnect(value));
}
