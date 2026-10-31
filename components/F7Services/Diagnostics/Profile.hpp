/**
 * @file Profile.hpp
 * @author Mark Stevens
 * @brief Record what the firmware is doing for the performance tools.
 * @date 2026-10-31
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 *
 * @note The code being measured calls Profile and does not know which tools are built in.
 *       With SystemView (build.sh --sysview) each call becomes an event in the Meadow
 *       SystemView module; without it every call compiles to nothing.
 */

#pragma once

#include "sdkconfig.h"

#include <stdint.h>

/**
 * @brief Events the firmware records.
 *
 * The values are the event numbers within the Meadow SystemView module.  They must match
 * the module description in Profile.cpp and tools/diagnostics/SYSVIEW_Meadow.txt.
 */
enum class ProfileEvent : uint32_t {
    /**
     * @brief An SPI transaction with the STM32 finished: the state it finished in and the
     *        number of bytes received.
     */
    SpiTransaction = 0,

    /**
     * @brief Start of working out what a finished SPI transaction means and preparing the
     *        next: the state.
     */
    SpiProcess = 1,

    /**
     * @brief End of SpiProcess: the state it started in.
     */
    SpiProcessDone = 2,

    /**
     * @brief A whole request has arrived from the STM32: message ID, interface, function.
     */
    RequestReceived = 3,

    /**
     * @brief A request handler starts on a request: message ID, interface, function.
     */
    HandleRequest = 4,

    /**
     * @brief End of HandleRequest: message ID.
     */
    HandleRequestDone = 5,

    /**
     * @brief A message was put on the queue for the STM32: message ID, message type, number
     *        of messages then on the queue.
     */
    MessageQueued = 6,

    /**
     * @brief The STM32 acknowledged the last packet of a message: message ID.
     */
    MessageAcknowledged = 7,

    /**
     * @brief Number of events; not an event.
     */
    NumberOfEvents = 8
};

/**
 * @brief Records events and spans for the performance tools.
 */
class Profile
{
public:
#if defined(CONFIG_MEADOW_SYSVIEW_EVENTS)

    /**
     * @brief Record an event with one value.
     *
     * @param event Event to record.
     * @param first Value recorded with the event.
     */
    static void Record(const ProfileEvent event, const uint32_t first);

    /**
     * @brief Record an event with two values.
     *
     * @param event Event to record.
     * @param first First value recorded with the event.
     * @param second Second value recorded with the event.
     */
    static void Record(const ProfileEvent event, const uint32_t first, const uint32_t second);

    /**
     * @brief Record an event with three values.
     *
     * @param event Event to record.
     * @param first First value recorded with the event.
     * @param second Second value recorded with the event.
     * @param third Third value recorded with the event.
     */
    static void Record(const ProfileEvent event, const uint32_t first, const uint32_t second, const uint32_t third);

    /**
     * @brief Mark the start of a message's life, so it can be followed across tasks.
     *
     * @param messageId ID of the message.
     */
    static void MessageStart(const uint32_t messageId);

    /**
     * @brief Mark the end of a message's life.
     *
     * @param messageId ID of the message.
     */
    static void MessageStop(const uint32_t messageId);

#else

    /**
     * @brief Record an event with one value (compiled out).
     */
    static inline void Record(const ProfileEvent, const uint32_t)
    {
    }

    /**
     * @brief Record an event with two values (compiled out).
     */
    static inline void Record(const ProfileEvent, const uint32_t, const uint32_t)
    {
    }

    /**
     * @brief Record an event with three values (compiled out).
     */
    static inline void Record(const ProfileEvent, const uint32_t, const uint32_t, const uint32_t)
    {
    }

    /**
     * @brief Mark the start of a message's life (compiled out).
     */
    static inline void MessageStart(const uint32_t)
    {
    }

    /**
     * @brief Mark the end of a message's life (compiled out).
     */
    static inline void MessageStop(const uint32_t)
    {
    }

#endif
};

/**
 * @brief Records a span for as long as the object exists.
 *
 * The span's start event is recorded when the object is made and its end event when it goes
 * out of scope, so every way out of the scope, including an early return, ends it.  The end
 * event carries the first value of the start event, so the two can be paired.
 *
 * @note A span ends with an event of its own rather than SystemView's end-of-call record,
 *       which ESP-IDF's sysviewtrace_proc.py cannot read.
 */
class ProfileSpan
{
public:
    /**
     * @brief Start a span with one value.
     *
     * @param start Event recorded at the start.
     * @param end Event recorded at the end.
     * @param first Value recorded with both.
     */
    ProfileSpan(const ProfileEvent start, const ProfileEvent end, const uint32_t first) : _end(end), _first(first)
    {
        Profile::Record(start, first);
    }

    /**
     * @brief Start a span with three values.
     *
     * @param start Event recorded at the start.
     * @param end Event recorded at the end, with the first value.
     * @param first First value recorded at the start.
     * @param second Second value recorded at the start.
     * @param third Third value recorded at the start.
     */
    ProfileSpan(const ProfileEvent start, const ProfileEvent end, const uint32_t first, const uint32_t second, const uint32_t third) : _end(end), _first(first)
    {
        Profile::Record(start, first, second, third);
    }

    /**
     * @brief End the span.
     */
    ~ProfileSpan()
    {
        Profile::Record(_end, _first);
    }

    /**
     * @brief A span cannot be copied: the copy would end it a second time.
     */
    ProfileSpan(const ProfileSpan &) = delete;

    /**
     * @brief A span cannot be assigned: the assignment would end it a second time.
     */
    ProfileSpan &operator=(const ProfileSpan &) = delete;

private:
    /**
     * @brief Event recorded at the end.
     */
    const ProfileEvent _end;

    /**
     * @brief First value of the start event, recorded again at the end.
     */
    const uint32_t _first;
};
