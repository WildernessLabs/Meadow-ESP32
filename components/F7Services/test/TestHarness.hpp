/**
 * @file TestHarness.hpp
 * @author Mark Stevens
 * @brief Message dispatcher lifetime and message flow methods shared by the tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_HARNESS_HPP_
#define _TEST_HARNESS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"

/**
 * @brief Flag indicating if heap tracing data should be generated.
 */
extern "C" bool _generateHeapTraceData;

/**
 * @brief Message dispatcher shared by the tests.
 *
 * @note MockMessageDispatcher holds the outbound message queue in a static member, so two
 *       live dispatchers share one queue and destroying either one invalidates it for the
 *       other.  One dispatcher is therefore created on demand and kept for the life of the
 *       test application; ResetMessageDispatcher clears it down between tests.
 */
extern MockMessageDispatcher *messageDispatcher;

/**
 * @brief Result of a socket call that returns a result and an errno.
 */
struct SocketCallResult
{
    /**
     * @brief Value the underlying POSIX call returned.
     */
    int32_t Result;

    /**
     * @brief Error number reported, using the NuttX numbering.
     */
    int32_t ResponseErrno;
};

/**
 * @brief Create the shared message dispatcher if it does not already exist.
 *
 * @return Pointer to the shared message dispatcher, nullptr if it cannot be created.
 */
MockMessageDispatcher *SetupMessageDispatcher(void);

/**
 * @brief Return the shared message dispatcher to a known state between tests.
 *
 * @note The dispatcher is not destroyed.  The request handlers cache it in a static that
 *       outlives any one test, and the Bluetooth handler raises events from its own task
 *       and stack callbacks, so destroying it would leave those references dangling.
 *       This releases the messages left in the outbound queue instead.
 */
void ResetMessageDispatcher(void);

/**
 * @brief Take every block of heap that can still be allocated, so the next allocation fails.
 *
 *  The blocks are chained through their own first bytes, so holding them needs no memory
 *  of its own.  Nothing that might allocate should run until ReleaseHeap is called.
 *
 * @return Head of the chain of blocks taken, for ReleaseHeap.
 */
void *ExhaustHeap(void);

/**
 * @brief Give back every block taken by ExhaustHeap.
 *
 * @param chain Head of the chain returned by ExhaustHeap.
 */
void ReleaseHeap(void *chain);

/**
 * @brief Wait for a message to arrive in the outbound queue, or for a timeout.
 *
 * @note This method allows for a request to take a time to respond.
 *
 * @param dispatcher Dispatcher whose outbound queue is being watched.
 * @param milliseconds Number of milliseconds to wait.
 */
void WaitForResponse(MockMessageDispatcher *dispatcher, uint32_t milliseconds);

/**
 * @brief Wait for the response to a given function, discarding anything else.
 *
 * Requests generate event messages as well as responses, and requests handled on the
 * thread pool do not respond immediately.  This drains the outbound queue until the
 * response to the given function arrives, releasing every other message it finds.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param interface Interface the response is expected to come from.
 * @param function Function the response is expected to carry.
 * @param timeout Milliseconds to wait for the response.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
Message *WaitForResponseTo(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, const unsigned timeout);

/**
 * @brief Queue a request and return the response it produced.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param interface Interface the request is for.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param timeout Milliseconds to wait for the response.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
Message *SendRequest(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, uint8_t *payload, const uint32_t payloadLength, const unsigned timeout);

/**
 * @brief Queue a request and read back the result and errno it produced.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param interface Interface the request is for.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param timeout Milliseconds to wait for the response.
 * @param callResult Filled in with the result and errno from the response.
 *
 * @return True if a response arrived, false if the call timed out.
 */
bool SendRequestForResultAndErrno(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, uint8_t *payload, const uint32_t payloadLength, const unsigned timeout, SocketCallResult *callResult);

/**
 * @brief Release every message waiting in the outbound queue.
 *
 * @param dispatcher Dispatcher whose outbound queue is to be emptied.
 * @param settlingTime Milliseconds to wait for any further messages after each one found.
 */
void DrainOutboundQueue(MockMessageDispatcher *dispatcher, const uint32_t settlingTime);

#endif // _TEST_HARNESS_HPP_
