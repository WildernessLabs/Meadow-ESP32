/**
 * @file TestWiFiHelpers.hpp
 * @author Mark Stevens
 * @brief WiFi setup and connection methods shared by the WiFi and POSIX tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_WIFI_HELPERS_HPP_
#define _TEST_WIFI_HELPERS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"

/**
 * @brief How a connection attempt progressed.
 */
struct ConnectionProgress
{
    /**
     * @brief True if a NetworkConnectedEvent was raised.
     */
    bool ConnectedEvent;

    /**
     * @brief True if the response to the connection request was received.
     */
    bool ResponseReceived;

    /**
     * @brief Status code the response to the connection request carried.
     */
    uint32_t ResponseStatusCode;
};

/**
 * @brief Prepare the message dispatcher the WiFi tests drive the radio through.
 *
 * The tests reach the WiFi request handler the way the STM32 does, by queueing messages
 * for the dispatcher.  The dispatcher creates the request handler the first time it has a
 * WiFi message to deliver and calls Setup() on it, so there is nothing for a test to
 * construct itself.
 *
 * @note One dispatcher serves every WiFi test in a run.  WiFiRequestHandler::Setup()
 *       creates a WiFi task and replaces the static event group the handler keeps its
 *       state in, so building a handler per test would leave a task behind each time.
 *       A test that needs the radio in a particular state therefore has to put it there
 *       rather than relying on a fresh handler to have reset it.
 *
 * @return Pointer to the dispatcher the tests should use.
 */
MockMessageDispatcher *SetupWiFiDispatcher(void);

/**
 * @brief Milliseconds to allow for a request that has to wait for the radio.
 *
 * @return Number of milliseconds to wait.
 */
unsigned RadioTimeout(void);

/**
 * @brief Create a message carrying an encoded AccessPointInformation object.
 *
 * This covers the requests that carry a network name and password, which are the requests
 * to connect to an access point and to start the soft access point.
 *
 * @param function Function the message should request.
 * @param ssid SSID of the access point.
 * @param password Password for the access point.
 * @param ipAddress Static IP address for the access point, 0 to use the default.
 * @param subnetMask Subnet mask for the access point, 0 to use the default.
 * @param gateway Gateway for the access point, 0 to use the default.
 *
 * @return Pointer to a message holding the encoded credentials in its payload.
 */
Message *CreateAccessPointMessage(const uint32_t function, const char *ssid, const char *password, const uint32_t ipAddress = 0, const uint32_t subnetMask = 0, const uint32_t gateway = 0);

/**
 * @brief Queue a request carrying a network name and password.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function the message should request.
 * @param ssid SSID of the access point.
 * @param password Password for the access point.
 * @param ipAddress Static IP address for the access point, 0 to use the default.
 * @param subnetMask Subnet mask for the access point, 0 to use the default.
 * @param gateway Gateway for the access point, 0 to use the default.
 */
void QueueAccessPointRequest(MockMessageDispatcher *dispatcher, const uint32_t function, const char *ssid, const char *password, const uint32_t ipAddress = 0, const uint32_t subnetMask = 0, const uint32_t gateway = 0);

/**
 * @brief Queue a request that carries no payload.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function the message should request.
 */
void QueueWiFiRequest(MockMessageDispatcher *dispatcher, const uint32_t function);

/**
 * @brief Read the messages a connection request generates until its response arrives.
 *
 * A connection request raises a number of events before the response to the request
 * itself is sent.  This reads them all, checking that each one is an event or a response
 * from the WiFi interface and that the events completed successfully, and stops once the
 * response to the connection request has been seen.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param timeout Milliseconds to wait for each message.
 * @param progress Filled in with the outcome of the connection attempt.
 */
void CollectConnectionMessages(MockMessageDispatcher *dispatcher, const unsigned timeout, ConnectionProgress *progress);

/**
 * @brief Stop the soft access point, whether or not it is running.
 *
 * Used by the tests that need the access point to be down before they start.  The
 * response is discarded, as a report that the access point was not running is as good an
 * outcome here as a report that it has been stopped.
 *
 * @param dispatcher Dispatcher to queue the request on.
 */
void EnsureAccessPointStopped(MockMessageDispatcher *dispatcher);

/**
 * @brief Disconnect from the access point, whether or not one is joined.
 *
 * Used by the tests that need the station to be down before they start.  A connection
 * left up by an earlier test is not merely untidy: ConnectToAccessPoint returns
 * WiFiAlreadyStarted without going near the radio once an address is held, so a test
 * that expects a connection attempt to be made would never see one.
 *
 * The response is discarded, as a report that nothing was connected is as good an
 * outcome here as a report that the connection has been dropped.
 *
 * @param dispatcher Dispatcher to queue the request on.
 */
void EnsureDisconnected(MockMessageDispatcher *dispatcher);

/**
 * @brief Connect to the access point named in the configuration.
 *
 * Socket creation is refused while the interface is down, so the POSIX tests cannot do
 * anything until this has succeeded.
 *
 * @note The first association after a reset regularly fails, the radio going from assoc
 *       back to init, and the next attempt then succeeds, so the request is retried.
 *
 * @note Whether the connection has been made is remembered here, so that the interface is
 *       only brought up once however many tests run.  The request handler keeps this state
 *       itself but does not expose it.
 *
 * @param dispatcher Dispatcher to queue the request on.
 *
 * @return True if the connection was made.
 */
bool ConnectToTestAccessPoint(MockMessageDispatcher *dispatcher);

#endif // _TEST_WIFI_HELPERS_HPP_
