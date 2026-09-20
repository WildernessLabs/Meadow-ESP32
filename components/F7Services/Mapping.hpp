/**
 * @file Mapping.hpp
 * @author Mark Stevens
 * @brief Define the class that will allow the mapping of values on the STM32 to those on the ESP32 and vice-versa.
 * @date 2020-05-19
 *
 * @copyright Copyright (c) 2020 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "SharedEnums.hpp"

/**
 *  @brief The Mapping class provides a mechanism to map STM32 constants to ESP32 constants and vice versa.
 */
class Mapping
{
public:
    /**
     *  @brief Type of mapping table to be used.
     */
    enum MappingTable { AddressFamily, SocketOptions, TCPOptions, ProtocolFamilies, SocketTypes, SocketLevel, Errno, MessageFlags, PollEvents, SocketShutdownType, EspErrorCodes, NetworkAuthenticationType, ResetReasons };

    /**
     *  @brief String mapping tables.
     */
    enum StringMappingTable { StatusCodes, MessageTypes, Interfaces, WiFiFunctions, SystemFunctions, BluetoothFunctions, TransportFunctions };

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
     *  @brief Hold an integer key and the string value representing the key.
     */
    struct str_mapping_s
    {
        int32_t key;
        const char *value;
    };
    typedef struct str_mapping_s str_mapping_t;

    /**
     *  @brief Table of address families (AF_ constants).
     */
    static int_mapping_t _addressFamilies[];

    /**
     *  @brief Table of socket options (SO_ constants).
     */
    static int_mapping_t _socketOptions[];

    /**
     * @brief Table of TCP options (TCP_ constants).
     */
    static int_mapping_t _tcpOptions[];

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
     *  @brief Table of the ESP Error codes mapping to the StatusCodes.
     */
    static int_mapping_t _espErrorCodes[];

    /**
     *  @brief Table of he ESP32 network authentication types mapping to the NetworkAuthenticationType enum.
     */
    static int_mapping_t _networkAuthenticationType[];

    /**
     *  @brief Table of the strings representing the StatusCodes.
     */
    static str_mapping_t _statusCodes[];

    /**
     *  @brief Table to reset codes.
     */
    static int_mapping_t _resetReasons[];

    /**
     *  @brief Message type mappings.
     */
    static str_mapping_t _messageTypeNames[];

    /**
     *  @brief Interface name mappings.
     */
    static str_mapping_t _interfaceNames[];

    /**
     *  @brief WiFi function mappings.
     */
    static str_mapping_t _wifiFunctionNames[];

    /**
     *  @brief System function mappings.
     */
    static str_mapping_t _systemFunctionNames[];

    /**
     *  @brief Bluetooth function mappings.
     */
    static str_mapping_t _bluetoothFunctionNames[];

    /**
     *  @brief Transport function mappings.
     */
    static str_mapping_t _transportFunctionNames[];

    /**
     *  @brief Get the mapped flags using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetFlagValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int32_t * const mappedValue);

    /**
     *  @brief Get the mapped flags using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetFlagValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int16_t * const mappedValue);

    /**
     *  @brief Get the mapped value using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int32_t * const result);

    /**
     *  @brief Get the mapped value using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int16_t * const result);

    /**
     *  @brief Get the mapped value using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int8_t * const result);

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
    static int32_t GetEspValue(const MappingTable mapping, const int32_t value, int8_t * const result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmValue(const MappingTable mapping, const int32_t value, int16_t * const result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmValue(const MappingTable mapping, const int32_t value, int8_t * const result);

    /**
     *  @brief Get the mapped value using the MappingDirection to determine which
     *  value should be retrieved.
     */
    static int32_t GetEspValue(const MappingTable mapping, const int32_t value, int16_t * const result);

    /**
     *  @brief Get the integer ESP32 value equivalent to the specified STM32 value.
     */
    static int32_t GetEspValue(const MappingTable mapping, const int32_t value, int32_t * const result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmValue(const MappingTable mapping, const int32_t value, int32_t * const result);

    /**
     *  @brief Get the integer ESP32 value equivalent to the specified STM32 value.
     */
    static int32_t GetEspFlagValue(const MappingTable mapping, const int32_t value, int32_t * const result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmFlagValue(const MappingTable mapping, const int32_t value, int32_t * const result);

    /**
     *  @brief Get the integer STM32 value equivalent to the specified ESP32 value.
     */
    static int32_t GetStmFlagValue(const MappingTable mapping, const int32_t value, int16_t * const result);

    /**
     *  @brief Get the string value for the given key.
     */
    static const char *GetStringValue(const StringMappingTable mapping, const int32_t key);

    /**
     *  @brief Map the ESP error codes to a StatusCode value.
     */
    static StatusCodes::StatusCodes GetStatusCode(const esp_err_t);

    /**
     *  @brief Map the ESP errno to a NuttX errno value.
     */
    static int32_t NuttxErrno(const int32_t espErrno);
};