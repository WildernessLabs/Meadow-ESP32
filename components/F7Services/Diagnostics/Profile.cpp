/**
 * @file Profile.cpp
 * @author Mark Stevens
 * @brief Record what the firmware is doing for the performance tools.
 * @date 2026-10-31
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include "Profile.hpp"

#if defined(CONFIG_MEADOW_SYSVIEW_EVENTS)

    #include <atomic>

    #include "SEGGER_SYSVIEW.h"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Send the rest of the Meadow module's description to the host.
 *
 * SystemView strings are limited to SEGGER_SYSVIEW_MAX_STRING_LEN (128) characters, so the
 * events are described in several parts.  The values are named for SystemView by
 * tools/diagnostics/SYSVIEW_Meadow.txt.
 */
static void SendModuleDescription();

/**
 * @brief The Meadow SystemView module.
 *
 * SystemView gives the module a block of event IDs when it is registered; the events are
 * recorded at EventOffset plus the ProfileEvent value.
 */
static SEGGER_SYSVIEW_MODULE _module = {"M=Meadow, 0 SpiTransaction state=%u bytes=%u, 1 SpiProcess state=%u, 2 SpiProcessDone state=%u", static_cast<U32>(ProfileEvent::NumberOfEvents), 0, SendModuleDescription, nullptr};

/**
 * @brief True once the module has been registered with SystemView.
 */
static std::atomic<bool> _registered(false);

/**
 * @brief True while the host has been sent the module's description for the current capture.
 */
static std::atomic<bool> _described(false);

/**
 * @brief Largest index of a registered module that is described again when a capture starts.
 */
static const U8 MAXIMUM_MODULE_INDEX = 8;

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

static void SendModuleDescription()
{
    SEGGER_SYSVIEW_RecordModuleDescription(&_module, "3 RequestReceived id=%p interface=%u function=%u, 4 HandleRequest id=%p interface=%u function=%u");
    SEGGER_SYSVIEW_RecordModuleDescription(&_module, "5 HandleRequestDone id=%p, 6 MessageQueued id=%p type=%u depth=%u, 7 MessageAcknowledged id=%p");
}

/**
 * @brief Make sure the host knows the Meadow module before an event is recorded.
 *
 * OpenOCD does not ask the ESP32 for its modules, so a module has to be sent once SystemView
 * has been started from the host, and sent again for each new capture.  A module registered
 * before the first capture would never be described.  Every registered module is sent, so
 * that one registered by ESP-IDF (heap tracing) is described too.
 *
 * @return true if a capture is running and the event should be recorded.
 */
static bool CaptureRunning()
{
    if (!SEGGER_SYSVIEW_Started())
    {
        _described.store(false, std::memory_order_relaxed);
        return (false);
    }
    if (!_described.exchange(true))
    {
        if (!_registered.exchange(true))
        {
            SEGGER_SYSVIEW_RegisterModule(&_module);
        }
        for (U8 index = 0; index <= MAXIMUM_MODULE_INDEX; index++)
        {
            SEGGER_SYSVIEW_SendModule(index);
        }
    }
    return (true);
}

/**
 * @brief SystemView event ID of a Meadow event.
 *
 * @param event Meadow event.
 * @return Event ID within the whole trace.
 */
static unsigned int EventId(const ProfileEvent event)
{
    return (_module.EventOffset + static_cast<unsigned int>(event));
}

/**
 * @brief Record an event with one value.
 *
 * @param event Event to record.
 * @param first Value recorded with the event.
 */
void Profile::Record(const ProfileEvent event, const uint32_t first)
{
    if (CaptureRunning())
    {
        SEGGER_SYSVIEW_RecordU32(EventId(event), first);
    }
}

/**
 * @brief Record an event with two values.
 *
 * @param event Event to record.
 * @param first First value recorded with the event.
 * @param second Second value recorded with the event.
 */
void Profile::Record(const ProfileEvent event, const uint32_t first, const uint32_t second)
{
    if (CaptureRunning())
    {
        SEGGER_SYSVIEW_RecordU32x2(EventId(event), first, second);
    }
}

/**
 * @brief Record an event with three values.
 *
 * @param event Event to record.
 * @param first First value recorded with the event.
 * @param second Second value recorded with the event.
 * @param third Third value recorded with the event.
 */
void Profile::Record(const ProfileEvent event, const uint32_t first, const uint32_t second, const uint32_t third)
{
    if (CaptureRunning())
    {
        SEGGER_SYSVIEW_RecordU32x3(EventId(event), first, second, third);
    }
}

/**
 * @brief Mark the start of a message's life, so it can be followed across tasks.
 *
 * The message ID is used as the SystemView marker, so the marker runs from the request
 * arriving to the STM32 acknowledging the response, whichever tasks handle it in between.
 *
 * @param messageId ID of the message.
 */
void Profile::MessageStart(const uint32_t messageId)
{
    if (CaptureRunning())
    {
        SEGGER_SYSVIEW_MarkStart(messageId);
    }
}

/**
 * @brief Mark the end of a message's life.
 *
 * @param messageId ID of the message.
 */
void Profile::MessageStop(const uint32_t messageId)
{
    if (CaptureRunning())
    {
        SEGGER_SYSVIEW_MarkStop(messageId);
    }
}

#endif // CONFIG_MEADOW_SYSVIEW_EVENTS
