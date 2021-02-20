/*
 *  Mapping.hpp
 *
 *  Define the class that will allow the mapping of values on the STM32 to
 *  those on the ESP32 and vice-versa.
 * 
 *  This file also contains the mappings.
 */
#ifndef MAIN_MAPPING_HPP
#define MAIN_MAPPING_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

/**
 *  @brief The Mapping class provides a mechanism to map STM32 constants to ESP32 constants and vice versa.
 */
class Mapping
{
public:
    /**
     *  @brief Type of mapping toable to be used.
     */
    enum MappingTable { AddressFamily, SocketOptions, ProtocolFamilies, SocketTypes, SocketLevel, Errno, MessageFlags, PollEvents,
                        SocketShutdownType };

private:
    /**
     *  @brief Mapping direction STM value to ESP value or ESP value to STM value.
     */
    enum MappingDirection { StmToEsp, EspToStm };

    /**
     *  @brief Hold the pairs of integers, one for the STM32 value, one for the ESP32 value.
     */
    struct int_mapping_s
    {
        int32_t stm;
        int32_t esp;
    };
    typedef struct int_mapping_s int_mapping_t;

    /**
     *  @brief Table of address families (AF_ constants).
     */
    static int_mapping_t _addressFamilies[];

    /**
     *  @brief Table of socket options (SO_ constants).
     */
    static int_mapping_t _socketOptions[];

    /**
     *  @brief Table of protocol families (PF_ constants).
     */
    static int_mapping_t _protocolFamilies[];

    /**
     *  @brief Table of socket types (SOCK_ constants).
     */
    static int_mapping_t _socketTypes[];

    /**
     *  @brief Table of socket levels (SOL_ constants).
     */
    static int_mapping_t _socketLevels[];

    /**
     *  @brief Table of error codes used to set the errno variable.
     */
    static int_mapping_t _errno[];

    /**
     *  @brief Table of polling events.
     */
    static int_mapping_t _pollEvents[];

    /**
     *  @brief Table of message flags
     */
    static int_mapping_t _messageFlags[];

    /**
     *  @brief Table of socket shutdown types.
     */
    static int_mapping_t _socketShutdownType[];

    /**
     *  @brief Get the mapped value using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetFlagValue(Mapping::MappingTable mapping, MappingDirection type, int32_t value, int32_t *mappedValue);

    /**
     *  @brief Get the mapped flags using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetValue(Mapping::MappingTable mapping, MappingDirection type, int32_t value, int32_t *result);

public:
    /**
     *  @brief Default constructor for the logging system.
     */
    Mapping();

    /**
     *  @brief Default destructor for the logging system.
     */
    ~Mapping();

    /**
     *  @brief Get the integer ESP32 value equivalent to the specified STM32 value.
     */
    static int32_t GetEspValue(Mapping::MappingTable mapping, int32_t value, int32_t *result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmValue(Mapping::MappingTable mapping, int32_t value, int32_t *result);

    /**
     *  @brief Get the integer ESP32 value equivalent to the specified STM32 value.
     */
    static int32_t GetEspFlagValue(Mapping::MappingTable mapping, int32_t value, int32_t *result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmFlagValue(Mapping::MappingTable mapping, int32_t value, int32_t *result);
};

#endif /* MAIN_MAPPING_HPP */