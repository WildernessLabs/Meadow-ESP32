/**
 * @file TestBluetoothHelpers.hpp
 * @author Mark Stevens
 * @brief Bluetooth service lifecycle methods used by the Bluetooth tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_BLUETOOTH_HELPERS_HPP_
#define _TEST_BLUETOOTH_HELPERS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "MockMessageDispatcher.hpp"
#include "SharedEnums.hpp"

/**
 * @brief Value used in place of a completion event when a request does not raise one.
 */
const int32_t NO_COMPLETION_EVENT = -1;

/**
 * @brief Milliseconds to wait for each message the Bluetooth service raises.
 */
const uint32_t BLUETOOTH_RESPONSE_TIMEOUT = 10000;

/**
 * @brief Make a request of the Bluetooth service and check the messages it raises.
 *
 * Starting and stopping the service both raise a transition event, then a completion event
 * if the request succeeded, and finally the response to the request itself.  Message
 * creation and delivery are multithreaded, so although the messages arrive in that order
 * the queue may hold more than one of them at any moment.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function being requested.
 * @param transitionEvent Event raised as the request starts to be handled.
 * @param completionEvent Event raised once the request has been handled, or
 *                        NO_COMPLETION_EVENT if the request does not raise one.
 * @param expectedStatusCode Status code the response to the request should carry.
 * @param payload Encoded request, nullptr if the request carries no payload.
 * @param payloadLength Length of the encoded request, 0 if the request carries no payload.
 */
void RunBluetoothLifecycleRequest(MockMessageDispatcher *dispatcher, const uint32_t function, const uint32_t transitionEvent, const int32_t completionEvent, const uint32_t expectedStatusCode, uint8_t *payload, const uint32_t payloadLength);

/**
 * @brief Start the Bluetooth service using the specified configuration.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param configuration Configuration of the service.
 */
void StartBluetoothService(MockMessageDispatcher *dispatcher, const char * const configuration);

/**
 * @brief Stop the Bluetooth service.
 *
 * @param dispatcher Dispatcher to queue the request on.
 */
void StopBluetoothService(MockMessageDispatcher *dispatcher);

/**
 * @brief Queue a request to write a value to a characteristic.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param handle Handle of the characteristic to write to.
 * @param value Value to write to the characteristic.
 */
void QueueServerDataSet(MockMessageDispatcher *dispatcher, const uint16_t handle, const uint32_t value);

/**
 * @brief Write a value to a characteristic and check the response to the request.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param handle Handle of the characteristic to write to.
 * @param value Value to write to the characteristic.
 * @param timeout Milliseconds to wait for the response.
 */
void WriteServerDataSet(MockMessageDispatcher *dispatcher, const uint16_t handle, const uint32_t value, const uint32_t timeout);

#endif // _TEST_BLUETOOTH_HELPERS_HPP_
