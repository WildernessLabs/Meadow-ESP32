/**
 * @file CoreDump.hpp
 * @author Wilderness Labs
 * @brief Core dump handling for the ESP32 application.
 * @date 2025-08-24
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note This file provides the interface for core dump management and analysis.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "IMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"

class CoreDump
{
public:
    /**
     * @brief CoreDump component name.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new Core Dump object
     */
    explicit CoreDump(IMessageDispatcher *messageDispatcher);

    /**
     * @brief Destroy the Core Dump object
     */
    ~CoreDump();

    //
    // Prevent move, copying and assignment.
    //
    CoreDump(const CoreDump &) = delete;
    CoreDump &operator=(const CoreDump &) = delete;
    CoreDump(CoreDump &&) = delete;
    CoreDump &operator=(CoreDump &&) = delete;

    /**
     * @brief Get the Information about the code dump.
     *
     * @param vpMessage Pointer to the message requesting the coredump information.
     */
    void GetInformation(void * const vpMessage);

    /**
     * @brief Get part (maybe all) of the core dump.
     *
     * @param vpMessage Pointer to the message requesting the core dump fragment.
     */
    void GetFragment(void * const vpMessage);

    /**
     * @brief Erase the core dump.
     *
     * @param vpMessage Pointer to the message requesting the core dump erasure.
     */
    void Erase(void * const vpMessage);

private:
    /**
     *  @brief Object that will be used to send messages to the STM32 from the ESP32.
     */
    IMessageDispatcher *_messageDispatcher;

    /**
     * @brief Construct a new Core Dump object
     */
    CoreDump() = delete;
};