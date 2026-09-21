/**
 * @file Encoders.cpp
 * @author Mark Stevens
 * @brief Message encoders and decoders implementation.
 * @date 2019-01-18
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "Encoders.hpp"
#include "Logging.hpp"
#include "SpiTransportProvider.hpp"
#include "Utils.hpp"

/**
 *  Calculate the amount of memory that should be allocated for
 *  the receive buffer.
 *
 *  SPI reception on the ESP32 should be on a 32-bit boundary and
 *  also a multiple of 4 bytes long (See the article linked below).
 *
 *  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
 *
 *  @param requestedSize
 *      Actual amount of data requested / to be received.
 *
 *  @returns
 *      Number of bytes that should be allocated.
 */
uint32_t Encoders::CalculateSpiBufferSize(const uint32_t requestedSize)
{
    uint32_t result = requestedSize;

    if (result < 8)
    {
        result = 8;
    }
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    //
    //  There is an issue with the SPI interface that can corrupt the last
    //  few bytes of a message to pad this message out and we will ignore
    //  the last few bytes when encoding / decoding.
    //
    result += Message::SPI_MESSAGE_OVERHEAD;
    return (result);
}

/**
 *  Calculate the minimum amount of space needed to store an encoded message.
 *
 *  Assumptions:
 *      1 - The packet information has been set up correctly.
 *
 *  @param message
 *      Message to be encoded.
 *
 *  @param headerOnly
 *      Are we just sending the header?
 *
 *  @returns
 *      Number of bytes needed to contain the message.
 */
uint32_t Encoders::EncodedPacketSize(const Message * const message, const bool headerOnly)
{
    uint32_t result = Message::HEADER_SIZE;

    if ((!headerOnly) && (message != NULL))
    {
        result += message->PacketLength;
    }

    return (result);
}

/**
 *  Take the first two bytes from the buffer and encode them as a 16 bit integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer holding the data that should be used to created the
 *      16-bit unsigned integer.
 *
 *  @returns
 *      16-bit unsigned integer value from the first two byte in the buffer.
 */
uint16_t Encoders::ExtractUInt16(const uint8_t * const buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/**
 *  Encode a 16-bit integer as two bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      16-bit value to encode in the first two bytes of the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where the two bytes will be inserted
 *      from the 16-bit unsigned integer (LSB first).
 */
void Encoders::EncodeUInt16(const uint16_t value, uint8_t * const buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/**
 *  Take the first four bytes from the buffer and encode them as a 32 bit unsigned integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer where the next four bytes should be extracted
 *      and a 32-bit unsigned integer created (LSB first).
 *
 *  @returns
 *      32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 */
uint32_t Encoders::ExtractUInt32(const uint8_t * const buffer)
{
    uint32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/**
 *  Encode a 32-bit unsigned integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void Encoders::EncodeUInt32(const uint32_t value, uint8_t * const buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Take the first four bytes from the buffer and encode them as a 32 bit integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer where the next four bytes should be extracted
 *      and a 32-bit unsigned integer created (LSB first).
 *
 *  @returns
 *      32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 */
uint32_t Encoders::ExtractInt32(const uint8_t * const buffer)
{
    int32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/**
 *  Encode a 32-bit  integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void Encoders::EncodeInt32(const int32_t value, uint8_t * const buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Extract a string (terminated by 0) from a block of memory.
 *
 *  buffer has no guaranteed trailing NUL of its own - it is typically a view into a
 *  message payload sized exactly to what was received on the wire.  The scan is bounded
 *  by maxLength so a payload that never contains a NUL cannot walk past the end of that
 *  block; nullptr is returned rather than treating an unterminated run as a string.
 *
 *  @param buffer
 *      Pointer to the block of memory containing the string.
 *
 *  @param maxLength
 *      Number of bytes, starting at buffer, that may be scanned looking for the
 *      terminator; the terminator itself must be one of those bytes.  Defaults to the
 *      largest a reassembled message payload can be, which bounds the resulting string's
 *      content to one byte less than that (the terminator takes the last slot).
 *
 *  @returns
 *      Pointer to the extracted string, or nullptr if buffer holds an empty string or no
 *      terminator was found within maxLength bytes.
 */
char *Encoders::ExtractString(const uint8_t * const buffer, const uint32_t maxLength)
{
    uint32_t length = 0;

    while ((length < maxLength) && (buffer[length] != 0))
    {
        length++;
    }

    char *result = nullptr;

    if ((length != 0) && (length < maxLength))
    {
        result = static_cast<char *>(pvPortMalloc(length + 1));
        if (result != NULL)
        {
            memcpy(result, buffer, length);
            result[length] = 0;
        }
    }
    return (result);
}

/**
 *  Copy the string into the buffer.
 *
 *  @param source
 *      Block of memory containing the string.
 *
 *  @param buffer
 *      Pointer to a block of memory to take the string.
 */
void Encoders::EncodeString(char *source, uint8_t *buffer)
{
    if (source == nullptr)
    {
        *buffer = 0;
    }
    else
    {
        strcpy(reinterpret_cast<char *>(buffer), source);
    }
}

/**
 *  Get the length of a string taking into consideration the
 *  fact that the string could be nullptr or NULL.
 *
 *  @param str
 *      Block of memory containing the string.
 *
 *  @returns
 *      Length of the string.
 */
uint32_t Encoders::StringLength(const char *str)
{
    uint32_t length = 0;

    if (str != nullptr)
    {
        length = strlen(str);
    }
    return (length);
}

/**
 *  Calculate the 8-bit CRC value for the specified data buffer.
 *
 *  This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      8-bit checksum of the bytes in the buffer.
 */
uint8_t Encoders::CRC8(const uint8_t *data, const uint16_t len)
{
    uint8_t crc = 0x00;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        uint8_t byte = data[byteCounter];
        for (uint8_t bitCounter = 8; bitCounter; bitCounter--)
        {
            uint8_t sum = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return (crc);
}

/**
 *  Calculate the 32-bit CRC of the buffer of data.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      32-bit checksum of the bytes in the buffer.
 */
uint32_t Encoders::CRC32(const uint8_t *data, const uint16_t len)
{
    uint32_t crc = CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = ProgressiveCRC32(crc, data[byteCounter]);
    }
    return (crc);
}

/**
 *  Calculate the CRC32 value change for the specified byte.
 *
 *  @param currentChecksum
 *      Current value of the checksum.
 *
 *  @param byte
 *      Next byte to use in the checksum calculation.
 *
 *  @returns
 *      Next checksum value.
 */
uint32_t Encoders::ProgressiveCRC32(const uint32_t currentChecksum, const uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        //
        //  We turn clang-format off on the following line as the CI tools like
        //  the line below but the local installed tools do complain that it is
        //  not formatted correctly.
        //
        // clang-format off
        uint32_t mask = - (crc & 0x01);
        // clang-format on
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}

/**
 *  @brief Extract a message from a block of memory and create a new Message object.
 *
 *  Note:
 *  There are four bytes at the end of the message that may be corrupted so
 *  we will pad out the message with four additional bytes and ignore these
 *  four bytes when calculating CRCs (we do not know what they will contain
 *  post transmission).
 *
 *  @param buffer
 *      Byte (uint8_t) array containing the encoded message.
 *
 *  @param bufferLength
 *      Size of the buffer.
 *
 *  @param headerOnly
 *      Indicate if we should extract the header information only
 *      (i.e. do not copy the payload).
 *
 *  @returns
 *      New Message object extracted from the buffer.  The Message object
 *      will be placed on the FreeRTOS heap (use Utils::DeleteAndNullifyPointer to release the
 *      storage).
 */
Message *Encoders::ExtractMessage(uint8_t *buffer, const uint32_t bufferLength, const bool headerOnly)
{
    uint32_t crc = ExtractUInt32(buffer + Message::CRC_OFFSET);
    EncodeUInt32(0, buffer + Message::CRC_OFFSET);

    Message *message = nullptr;
    if (crc == CRC32(buffer, bufferLength - 4)) // See note in comment above.
    {
        message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));

        if (message == NULL)
        {
            return (nullptr);
        }

        memset(static_cast<void *>(message), 0, sizeof(Message));
        buffer += 5; // Skip the protocol and CRC.
        message->PacketOffset = ExtractUInt16(buffer);
        buffer += 2;
        message->PacketLength = ExtractUInt16(buffer);
        buffer += 2;
        message->MessageType = *buffer;
        buffer++;
        message->Interface = *buffer;
        buffer++;
        message->Function = ExtractUInt32(buffer);
        buffer += 4;
        message->StatusCode = ExtractUInt32(buffer);
        buffer += 4;
        message->MessageID = ExtractUInt32(buffer);
        buffer += 4;
        message->PayloadLength = ExtractUInt32(buffer);
        buffer += 4;
        if (!headerOnly && (message->PayloadLength > 0))
        {
            message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PacketLength));
            if (message->Payload == NULL)
            {
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&message));
            }
            else
            {
                memcpy(message->Payload, buffer, message->PacketLength);
            }
        }
        else
        {
            message->Payload = nullptr;
        }
    }
    return (message);
}

/**
 *  @brief Encode a message into a block of memory.
 *
 *  @param message
 *      Message structure to encode in to the byte buffer.
 *
 *  @param buffer
 *      Pointer to the block of memory that will contain the encoded message.
 *
 *  @param headerOnly
 *      Copy this as a header only (i.e. do not copy the payload).
 *
 *  @returns
 *     Byte buffer containing the encoded message.  The storage
 *     Will be placed on the FreeRTOS heap (use Utils::DeleteAndNullifyPointer to release the
 *     storage).
 */
void Encoders::EncodeMessage(const Message * const message, uint8_t * const buffer, const bool headerOnly)
{
    if (buffer)
    {
        uint32_t messageSize = EncodedPacketSize(message, headerOnly);
        uint32_t bufferSize = Encoders::CalculateSpiBufferSize(messageSize);
        TRACE_MESSAGE_SPECIFY_COMPONENT(SpiTransportProvider::COMPONENT_NAME, "Encoding %" PRIu32 " bytes as a %" PRIu32 " byte packet", messageSize, bufferSize);

        uint8_t *next_location = buffer;

        memset(buffer, 0, bufferSize);
        *next_location = Message::PROTOCOL_NUMBER;
        next_location++;
        EncodeUInt32(0, next_location);
        next_location += 4;
        EncodeUInt16(message->PacketOffset, next_location);
        next_location += 2;
        EncodeUInt16(message->PacketLength, next_location);
        next_location += 2;
        *next_location = message->MessageType;
        next_location++;
        *next_location = message->Interface;
        next_location++;
        EncodeUInt32(message->Function, next_location);
        next_location += 4;
        EncodeUInt32(message->StatusCode, next_location);
        next_location += 4;
        EncodeUInt32(message->MessageID, next_location);
        next_location += 4;
        EncodeUInt32(message->PayloadLength, next_location);
        next_location += 4;
        if (!headerOnly && (message->PacketLength > 0))
        {
            memcpy(next_location, message->Payload + message->PacketOffset, message->PacketLength);
        }
        uint32_t crc = CRC32(buffer, bufferSize - 4);
        EncodeUInt32(crc, buffer + Message::CRC_OFFSET);
    }
}

/**
 *  Convert the SystemConfiguration object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedSystemConfigurationBufferSize method.
 *
 *  @param systemConfiguration
 *      SystemConfiguration object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SystemConfiguration object.
 */
void Encoders::EncodeSystemConfiguration(const Esp32Messaging::SystemConfiguration * const systemConfiguration, uint8_t *buffer)
{
    *buffer = systemConfiguration->MaximumMessageQueueLength;
    buffer += 1;
    EncodeInt32(systemConfiguration->MaximumRetryCount, buffer);
    buffer += 4;
    *buffer = systemConfiguration->Antenna;
    buffer += 1;
    memcpy(buffer, systemConfiguration->BoardMacAddress, 6);
    buffer += 6;
    memcpy(buffer, systemConfiguration->SoftApMacAddress, 6);
    buffer += 6;
    memcpy(buffer, systemConfiguration->BluetoothMacAddress, 6);
    buffer += 6;
    EncodeString(systemConfiguration->DeviceName, buffer);
    buffer += StringLength(systemConfiguration->DeviceName) + 1;
    EncodeString(systemConfiguration->DefaultAccessPoint, buffer);
    buffer += StringLength(systemConfiguration->DefaultAccessPoint) + 1;
    *buffer = systemConfiguration->ResetReason;
    buffer += 1;
    EncodeUInt32(systemConfiguration->VersionMajor, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionMinor, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionRevision, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionBuild, buffer);
    buffer += 4;
    *buffer = systemConfiguration->BuildDay;
    buffer += 1;
    *buffer = systemConfiguration->BuildMonth;
    buffer += 1;
    *buffer = systemConfiguration->BuildYear;
    buffer += 1;
    *buffer = systemConfiguration->BuildHour;
    buffer += 1;
    *buffer = systemConfiguration->BuildMinute;
    buffer += 1;
    *buffer = systemConfiguration->BuildSecond;
    buffer += 1;
    EncodeUInt32(systemConfiguration->BuildHash, buffer);
    buffer += 4;
    EncodeString(systemConfiguration->BuildBranchName, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SystemConfiguration object.
 *
 *  @param systemConfiguration
 *      SystemConfiguration object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded SystemConfiguration object.
 */
int Encoders::EncodedSystemConfigurationBufferSize(const Esp32Messaging::SystemConfiguration * const systemConfiguration)
{
    int result = 0;
    result += Encoders::StringLength(systemConfiguration->DeviceName);
    result += Encoders::StringLength(systemConfiguration->DefaultAccessPoint);
    result += Encoders::StringLength(systemConfiguration->BuildBranchName);
    return (result + 54);
}

/**
 *  Extract the ConfigurationValue object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeConfigurationValue(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConfigurationValue object.
 *
 *  @returns
 *      Pointer to a ConfigurationValue object.
 */
Esp32Messaging::ConfigurationValue *Encoders::ExtractConfigurationValue(uint8_t *buffer)
{
    if (buffer == nullptr)
    {
        return (nullptr);
    }

    Esp32Messaging::ConfigurationValue *configurationValue = static_cast<Esp32Messaging::ConfigurationValue *>(pvPortMalloc(sizeof(Esp32Messaging::ConfigurationValue)));

    if (configurationValue == NULL)
    {
        return (nullptr);
    }

    configurationValue->Item = ExtractUInt32(buffer);
    buffer += 4;
    configurationValue->ValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (configurationValue->ValueLength > 0)
    {
        configurationValue->Value = static_cast<uint8_t *>(pvPortMalloc(configurationValue->ValueLength));

        if (configurationValue->Value == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&configurationValue));
            return (nullptr);
        }

        memcpy(configurationValue->Value, buffer, configurationValue->ValueLength);
        buffer += configurationValue->ValueLength;
    }
    else
    {
        configurationValue->Value = nullptr;
    }
    return (configurationValue);
}

/**
 *  Release the heap storage used by a ConfigurationValue object along with the storage used by
 *  the nested Value member.
 *
 *  @param configurationValue
 *      Pointer to the pointer to the ConfigurationValue object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeConfigurationValue(Esp32Messaging::ConfigurationValue **configurationValue)
{
    if ((configurationValue == nullptr) || (*configurationValue == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*configurationValue)->Value));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(configurationValue));
}

/**
 *  Convert the AccessPointInformation object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedAccessPointInformationBufferSize method.
 *
 *  @param accessPointInformation
 *      AccessPointInformation object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPointInformation object.
 */
void Encoders::EncodeAccessPointInformation(const Esp32Messaging::AccessPointInformation * const accessPointInformation, uint8_t *buffer)
{
    EncodeString(accessPointInformation->NetworkName, buffer);
    buffer += StringLength(accessPointInformation->NetworkName) + 1;
    EncodeString(accessPointInformation->Password, buffer);
    buffer += StringLength(accessPointInformation->Password) + 1;
    EncodeUInt32(accessPointInformation->IpAddress, buffer);
    buffer += 4;
    EncodeUInt32(accessPointInformation->SubnetMask, buffer);
    buffer += 4;
    EncodeUInt32(accessPointInformation->Gateway, buffer);
    buffer += 4;
    *buffer = accessPointInformation->WiFiAuthenticationMode;
    buffer += 1;
    *buffer = accessPointInformation->Channel;
    buffer += 1;
    *buffer = accessPointInformation->Hidden;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPointInformation object.
 *
 *  @param accessPointInformation
 *      AccessPointInformation object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded AccessPointInformation object.
 */
int Encoders::EncodedAccessPointInformationBufferSize(const Esp32Messaging::AccessPointInformation * const accessPointInformation)
{
    int result = 0;
    result += Encoders::StringLength(accessPointInformation->NetworkName);
    result += Encoders::StringLength(accessPointInformation->Password);
    return (result + 17);
}

/**
 *  Extract the AccessPointInformation object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeAccessPointInformation(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointInformation object.
 *
 *  @returns
 *      Pointer to a AccessPointInformation object.
 */
Esp32Messaging::AccessPointInformation *Encoders::ExtractAccessPointInformation(uint8_t *buffer)
{
    Esp32Messaging::AccessPointInformation *accessPointInformation = static_cast<Esp32Messaging::AccessPointInformation *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointInformation)));

    if (accessPointInformation == NULL)
    {
        return (nullptr);
    }

    accessPointInformation->NetworkName = ExtractString(buffer);
    buffer += Encoders::StringLength(accessPointInformation->NetworkName) + 1;
    accessPointInformation->Password = ExtractString(buffer);
    buffer += Encoders::StringLength(accessPointInformation->Password) + 1;
    accessPointInformation->IpAddress = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->SubnetMask = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->Gateway = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->WiFiAuthenticationMode = *buffer;
    buffer += 1;
    accessPointInformation->Channel = *buffer;
    buffer += 1;
    accessPointInformation->Hidden = *buffer;
    return (accessPointInformation);
}

/**
 *  Release the heap storage used by an AccessPointInformation object along with the storage used by
 *  the nested NetworkName and Password members.
 *
 *  @param accessPointInformation
 *      Pointer to the pointer to the AccessPointInformation object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeAccessPointInformation(Esp32Messaging::AccessPointInformation **accessPointInformation)
{
    if ((accessPointInformation == nullptr) || (*accessPointInformation == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*accessPointInformation)->NetworkName));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*accessPointInformation)->Password));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(accessPointInformation));
}

/**
 *  Extract the DisconnectFromAccessPointRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded DisconnectFromAccessPointRequest object.
 *
 *  @returns
 *      Pointer to a DisconnectFromAccessPointRequest object.
 */
Esp32Messaging::DisconnectFromAccessPointRequest *Encoders::ExtractDisconnectFromAccessPointRequest(uint8_t *buffer)
{
    Esp32Messaging::DisconnectFromAccessPointRequest *disconnectFromAccessPointRequest = static_cast<Esp32Messaging::DisconnectFromAccessPointRequest *>(pvPortMalloc(sizeof(Esp32Messaging::DisconnectFromAccessPointRequest)));

    if (disconnectFromAccessPointRequest == NULL)
    {
        return (nullptr);
    }

    disconnectFromAccessPointRequest->TurnOffWiFiInterface = *buffer;
    return (disconnectFromAccessPointRequest);
}

/**
 *  Convert the ConnectEventData object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedConnectEventDataBufferSize method.
 *
 *  @param connectEventData
 *      ConnectEventData object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ConnectEventData object.
 */
void Encoders::EncodeConnectEventData(const Esp32Messaging::ConnectEventData * const connectEventData, uint8_t *buffer)
{
    EncodeUInt32(connectEventData->IpAddress, buffer);
    buffer += 4;
    EncodeUInt32(connectEventData->SubnetMask, buffer);
    buffer += 4;
    EncodeUInt32(connectEventData->Gateway, buffer);
    buffer += 4;
    memcpy(buffer, connectEventData->Ssid, 33);
    buffer += 33;
    memcpy(buffer, connectEventData->Bssid, 6);
    buffer += 6;
    *buffer = connectEventData->Channel;
    buffer += 1;
    *buffer = connectEventData->AuthenticationMode;
    buffer += 1;
    EncodeUInt32(connectEventData->Reason, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ConnectEventData object.
 *
 *  @param connectEventData
 *      ConnectEventData object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded ConnectEventData object.
 */
int Encoders::EncodedConnectEventDataBufferSize(const Esp32Messaging::ConnectEventData * const connectEventData)
{
    return (57);
}

/**
 *  Convert the DisconnectEventData object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodeDisconnectEventDataBufferSize method.
 *
 *  @param disconnectEventData
 *      DisconnectEventData object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded DisconnectEventData object.
 */
void Encoders::EncodeDisconnectEventData(const Esp32Messaging::DisconnectEventData * const disconnectEventData, uint8_t *buffer)
{
    memcpy(buffer, disconnectEventData->Ssid, 33);
    buffer += 33;
    *buffer = disconnectEventData->SsidLength;
    buffer += 1;
    memcpy(buffer, disconnectEventData->Bssid, 6);
    buffer += 6;
    *buffer = disconnectEventData->Rssi;
    buffer += 1;
    *buffer = disconnectEventData->Reason;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the DisconnectEventData object.
 *
 *  @param disconnectEventData
 *      DisconnectEventData object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded DisconnectEventData object.
 */
int Encoders::EncodeDisconnectEventDataBufferSize(const Esp32Messaging::DisconnectEventData * const disconnectEventData)
{
    return (42);
}

/**
 *  Convert the NodeConnectionChangeEventData object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedNodeConnectionChangeEventDataBufferSize method.
 *
 *  @param nodeConnectionChangeEventData
 *      NodeConnectionChangeEventData object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded NodeConnectionChangeEventData object.
 */
void Encoders::EncodeNodeConnectionChangeEventData(const Esp32Messaging::NodeConnectionChangeEventData * const nodeConnectionChangeEventData, uint8_t *buffer)
{
    EncodeUInt32(nodeConnectionChangeEventData->IpAddress, buffer);
    buffer += 4;
    memcpy(buffer, nodeConnectionChangeEventData->MacAddress, 6);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the NodeConnectionChangeEventData object.
 *
 *  @param nodeConnectionChangeEventData
 *      NodeConnectionChangeEventData object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded NodeConnectionChangeEventData object.
 */
int Encoders::EncodedNodeConnectionChangeEventDataBufferSize(const Esp32Messaging::NodeConnectionChangeEventData * const nodeConnectionChangeEventData)
{
    return (10);
}

/**
 *  Convert the AccessPoint object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedAccessPointBufferSize method.
 *
 *  @param accessPoint
 *      AccessPoint object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPoint object.
 */
void Encoders::EncodeAccessPoint(const Esp32Messaging::AccessPoint * const accessPoint, uint8_t *buffer)
{
    memcpy(buffer, accessPoint->Ssid, 33);
    buffer += 33;
    memcpy(buffer, accessPoint->Bssid, 6);
    buffer += 6;
    *buffer = accessPoint->PrimaryChannel;
    buffer += 1;
    *buffer = accessPoint->SecondaryChannel;
    buffer += 1;
    *buffer = static_cast<uint8_t>(accessPoint->Rssi);
    buffer += 1;
    *buffer = accessPoint->AuthenticationMode;
    buffer += 1;
    EncodeUInt32(accessPoint->Protocols, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPoint object.
 *
 *  @param accessPoint
 *      AccessPoint object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded AccessPoint object.
 */
int Encoders::EncodedAccessPointBufferSize(const Esp32Messaging::AccessPoint * const accessPoint)
{
    return (47);
}

/**
 *  Convert the AccessPointList object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedAccessPointListBufferSize method.
 *
 *  @param accessPointList
 *      AccessPointList object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPointList object.
 */
void Encoders::EncodeAccessPointList(const Esp32Messaging::AccessPointList * const accessPointList, uint8_t *buffer)
{
    EncodeUInt32(accessPointList->NumberOfAccessPoints, buffer);
    buffer += 4;
    EncodeUInt32(accessPointList->AccessPointsLength, buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        memcpy(buffer, accessPointList->AccessPoints, accessPointList->AccessPointsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPointList object.
 *
 *  @param accessPointList
 *      AccessPointList object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded AccessPointList object.
 */
int Encoders::EncodedAccessPointListBufferSize(const Esp32Messaging::AccessPointList * const accessPointList)
{
    int result = 0;
    result += accessPointList->AccessPointsLength;
    return (result + 8);
}

/**
 *  Convert the SockAddr object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedSockAddrBufferSize method.
 *
 *  @param sockAddr
 *      SockAddr object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SockAddr object.
 */
void Encoders::EncodeSockAddr(const Esp32Messaging::SockAddr * const sockAddr, uint8_t *buffer)
{
    *buffer = sockAddr->Family;
    buffer += 1;
    EncodeUInt16(sockAddr->Port, buffer);
    buffer += 2;
    EncodeUInt32(sockAddr->Ip4Address, buffer);
    buffer += 4;
    EncodeUInt32(sockAddr->FlowInfo, buffer);
    buffer += 4;
    memcpy(buffer, sockAddr->Ip6Address, 16);
    buffer += 16;
    EncodeUInt32(sockAddr->ScopeID, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SockAddr object.
 *
 *  @param sockAddr
 *      SockAddr object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded SockAddr object.
 */
int Encoders::EncodedSockAddrBufferSize(const Esp32Messaging::SockAddr * const sockAddr)
{
    return (31);
}

/**
 *  Extract the SockAddr object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SockAddr object.
 *
 *  @returns
 *      Pointer to a SockAddr object.
 */
Esp32Messaging::SockAddr *Encoders::ExtractSockAddr(uint8_t *buffer)
{
    Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));

    if (sockAddr == NULL)
    {
        return (nullptr);
    }

    sockAddr->Family = *buffer;
    buffer += 1;
    sockAddr->Port = ExtractUInt16(buffer);
    buffer += 2;
    sockAddr->Ip4Address = ExtractUInt32(buffer);
    buffer += 4;
    sockAddr->FlowInfo = ExtractUInt32(buffer);
    buffer += 4;
    memcpy(sockAddr->Ip6Address, buffer, 16);
    buffer += 16;
    sockAddr->ScopeID = ExtractUInt32(buffer);
    return (sockAddr);
}

/**
 *  Convert the AddrInfo object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedAddrInfoBufferSize method.
 *
 *  @param addrInfo
 *      AddrInfo object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AddrInfo object.
 */
void Encoders::EncodeAddrInfo(const Esp32Messaging::AddrInfo * const addrInfo, uint8_t *buffer)
{
    EncodeUInt32(addrInfo->MyHeapAddress, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Flags, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Family, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->SocketType, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Protocol, buffer);
    buffer += 4;
    EncodeUInt32(addrInfo->AddrLen, buffer);
    buffer += 4;
    EncodeUInt32(addrInfo->AddrLength, buffer);
    buffer += 4;
    if (addrInfo->AddrLength > 0)
    {
        memcpy(buffer, addrInfo->Addr, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    EncodeString(addrInfo->CanonName, buffer);
    buffer += StringLength(addrInfo->CanonName) + 1;
    EncodeUInt32(reinterpret_cast<uint32_t>(addrInfo->Next), buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AddrInfo object.
 *
 *  @param addrInfo
 *      AddrInfo object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded AddrInfo object.
 */
int Encoders::EncodedAddrInfoBufferSize(const Esp32Messaging::AddrInfo * const addrInfo)
{
    int result = 0;
    result += addrInfo->AddrLength;
    result += Encoders::StringLength(addrInfo->CanonName);
    return (result + 33);
}

/**
 *  Extract the AddrInfo object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeAddrInfo(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AddrInfo object.
 *
 *  @returns
 *      Pointer to a AddrInfo object.
 */
Esp32Messaging::AddrInfo *Encoders::ExtractAddrInfo(uint8_t *buffer)
{
    Esp32Messaging::AddrInfo *addrInfo = static_cast<Esp32Messaging::AddrInfo *>(pvPortMalloc(sizeof(Esp32Messaging::AddrInfo)));

    if (addrInfo == NULL)
    {
        return (nullptr);
    }

    addrInfo->MyHeapAddress = ExtractUInt32(buffer);
    buffer += 4;
    addrInfo->Flags = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->Family = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->SocketType = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->Protocol = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->AddrLen = ExtractUInt32(buffer);
    buffer += 4;
    addrInfo->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (addrInfo->AddrLength > 0)
    {
        addrInfo->Addr = static_cast<uint8_t *>(pvPortMalloc(addrInfo->AddrLength));
        if (addrInfo->Addr == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&addrInfo));
            return (nullptr);
        }
        memcpy(addrInfo->Addr, buffer, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    else
    {
        addrInfo->Addr = nullptr;
    }
    addrInfo->CanonName = ExtractString(buffer);
    buffer += Encoders::StringLength(addrInfo->CanonName) + 1;
    addrInfo->Next = reinterpret_cast<void *>(ExtractUInt32(buffer));
    return (addrInfo);
}

/**
 *  Release the heap storage used by an AddrInfo object along with the storage used by
 *  the nested Addr and CanonName members.
 *
 *  @param addrInfo
 *      Pointer to the pointer to the AddrInfo object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeAddrInfo(Esp32Messaging::AddrInfo **addrInfo)
{
    if ((addrInfo == nullptr) || (*addrInfo == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*addrInfo)->Addr));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*addrInfo)->CanonName));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(addrInfo));
}

/**
 *  Extract the GetAddrInfoRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeGetAddrInfoRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetAddrInfoRequest object.
 *
 *  @returns
 *      Pointer to a GetAddrInfoRequest object.
 */
Esp32Messaging::GetAddrInfoRequest *Encoders::ExtractGetAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest = static_cast<Esp32Messaging::GetAddrInfoRequest *>(pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoRequest)));

    if (getAddrInfoRequest == NULL)
    {
        return (nullptr);
    }

    getAddrInfoRequest->NodeName = ExtractString(buffer);
    buffer += Encoders::StringLength(getAddrInfoRequest->NodeName) + 1;
    getAddrInfoRequest->ServName = ExtractString(buffer);
    buffer += Encoders::StringLength(getAddrInfoRequest->ServName) + 1;
    getAddrInfoRequest->HintsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoRequest->HintsLength > 0)
    {
        getAddrInfoRequest->Hints = static_cast<uint8_t *>(pvPortMalloc(getAddrInfoRequest->HintsLength));
        if (getAddrInfoRequest->Hints == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&getAddrInfoRequest));
            return (nullptr);
        }
        memcpy(getAddrInfoRequest->Hints, buffer, getAddrInfoRequest->HintsLength);
        buffer += getAddrInfoRequest->HintsLength;
    }
    else
    {
        getAddrInfoRequest->Hints = nullptr;
    }
    return (getAddrInfoRequest);
}

/**
 *  Release the heap storage used by a GetAddrInfoRequest object along with the storage used by
 *  the nested NodeName, ServName and Hints members.
 *
 *  @param getAddrInfoRequest
 *      Pointer to the pointer to the GetAddrInfoRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeGetAddrInfoRequest(Esp32Messaging::GetAddrInfoRequest **getAddrInfoRequest)
{
    if ((getAddrInfoRequest == nullptr) || (*getAddrInfoRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*getAddrInfoRequest)->NodeName));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*getAddrInfoRequest)->ServName));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*getAddrInfoRequest)->Hints));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(getAddrInfoRequest));
}

/**
 *  Convert the GetAddrInfoResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedGetAddrInfoResponseBufferSize method.
 *
 *  @param getAddrInfoResponse
 *      GetAddrInfoResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetAddrInfoResponse object.
 */
void Encoders::EncodeGetAddrInfoResponse(const Esp32Messaging::GetAddrInfoResponse * const getAddrInfoResponse, uint8_t *buffer)
{
    EncodeInt32(getAddrInfoResponse->AddrInfoResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(getAddrInfoResponse->ResLength, buffer);
    buffer += 4;
    if (getAddrInfoResponse->ResLength > 0)
    {
        memcpy(buffer, getAddrInfoResponse->Res, getAddrInfoResponse->ResLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetAddrInfoResponse object.
 *
 *  @param getAddrInfoResponse
 *      GetAddrInfoResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded GetAddrInfoResponse object.
 */
int Encoders::EncodedGetAddrInfoResponseBufferSize(const Esp32Messaging::GetAddrInfoResponse * const getAddrInfoResponse)
{
    int result = 0;
    result += getAddrInfoResponse->ResLength;
    return (result + 8);
}

/**
 *  Extract the SocketRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SocketRequest object.
 *
 *  @returns
 *      Pointer to a SocketRequest object.
 */
Esp32Messaging::SocketRequest *Encoders::ExtractSocketRequest(uint8_t *buffer)
{
    Esp32Messaging::SocketRequest *socketRequest = static_cast<Esp32Messaging::SocketRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SocketRequest)));

    if (socketRequest == NULL)
    {
        return (nullptr);
    }

    socketRequest->AddressInformation = reinterpret_cast<void *>(ExtractUInt32(buffer));
    buffer += 4;
    socketRequest->Domain = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Type = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Protocol = ExtractInt32(buffer);
    return (socketRequest);
}

/**
 *  Convert the IntegerResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedIntegerResponseBufferSize method.
 *
 *  @param integerResponse
 *      IntegerResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IntegerResponse object.
 */
void Encoders::EncodeIntegerResponse(const Esp32Messaging::IntegerResponse * const integerResponse, uint8_t *buffer)
{
    EncodeInt32(integerResponse->Result, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IntegerResponse object.
 *
 *  @param integerResponse
 *      IntegerResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded IntegerResponse object.
 */
int Encoders::EncodedIntegerResponseBufferSize(const Esp32Messaging::IntegerResponse * const integerResponse)
{
    return (4);
}

/**
 *  Convert the IntegerAndErrnoResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedIntegerAndErrnoResponseBufferSize method.
 *
 *  @param integerAndErrnoResponse
 *      IntegerAndErrnoResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IntegerAndErrnoResponse object.
 */
void Encoders::EncodeIntegerAndErrnoResponse(const Esp32Messaging::IntegerAndErrnoResponse * const integerAndErrnoResponse, uint8_t *buffer)
{
    EncodeInt32(integerAndErrnoResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(integerAndErrnoResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IntegerAndErrnoResponse object.
 *
 *  @param integerAndErrnoResponse
 *      IntegerAndErrnoResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded IntegerAndErrnoResponse object.
 */
int Encoders::EncodedIntegerAndErrnoResponseBufferSize(const Esp32Messaging::IntegerAndErrnoResponse * const integerAndErrnoResponse)
{
    return (8);
}

/**
 *  Extract the ConnectRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeConnectRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConnectRequest object.
 *
 *  @returns
 *      Pointer to a ConnectRequest object.
 */
Esp32Messaging::ConnectRequest *Encoders::ExtractConnectRequest(uint8_t *buffer)
{
    Esp32Messaging::ConnectRequest *connectRequest = static_cast<Esp32Messaging::ConnectRequest *>(pvPortMalloc(sizeof(Esp32Messaging::ConnectRequest)));

    if (connectRequest == NULL)
    {
        return (nullptr);
    }

    connectRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    connectRequest->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (connectRequest->AddrLength > 0)
    {
        connectRequest->Addr = static_cast<uint8_t *>(pvPortMalloc(connectRequest->AddrLength));
        if (connectRequest->Addr == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&connectRequest));
            return (nullptr);
        }
        memcpy(connectRequest->Addr, buffer, connectRequest->AddrLength);
        buffer += connectRequest->AddrLength;
    }
    else
    {
        connectRequest->Addr = nullptr;
    }
    return (connectRequest);
}

/**
 *  Release the heap storage used by a ConnectRequest object along with the storage used by
 *  the nested Addr member.
 *
 *  @param connectRequest
 *      Pointer to the pointer to the ConnectRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeConnectRequest(Esp32Messaging::ConnectRequest **connectRequest)
{
    if ((connectRequest == nullptr) || (*connectRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*connectRequest)->Addr));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(connectRequest));
}

/**
 *  Extract the FreeAddrInfoRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FreeAddrInfoRequest object.
 *
 *  @returns
 *      Pointer to a FreeAddrInfoRequest object.
 */
Esp32Messaging::FreeAddrInfoRequest *Encoders::ExtractFreeAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest = static_cast<Esp32Messaging::FreeAddrInfoRequest *>(pvPortMalloc(sizeof(Esp32Messaging::FreeAddrInfoRequest)));

    if (freeAddrInfoRequest == NULL)
    {
        return (nullptr);
    }

    freeAddrInfoRequest->AddrInfoAddress = ExtractUInt32(buffer);
    return (freeAddrInfoRequest);
}

/**
 *  Convert the TimeVal object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedTimeValBufferSize method.
 *
 *  @param timeVal
 *      TimeVal object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded TimeVal object.
 */
void Encoders::EncodeTimeVal(const Esp32Messaging::TimeVal * const timeVal, uint8_t *buffer)
{
    EncodeUInt32(timeVal->TvSec, buffer);
    buffer += 4;
    EncodeUInt32(timeVal->TvUsec, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the TimeVal object.
 *
 *  @param timeVal
 *      TimeVal object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded TimeVal object.
 */
int Encoders::EncodedTimeValBufferSize(const Esp32Messaging::TimeVal * const timeVal)
{
    return (8);
}

/**
 *  Extract the TimeVal object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded TimeVal object.
 *
 *  @returns
 *      Pointer to a TimeVal object.
 */
Esp32Messaging::TimeVal *Encoders::ExtractTimeVal(uint8_t *buffer)
{
    Esp32Messaging::TimeVal *timeVal = static_cast<Esp32Messaging::TimeVal *>(pvPortMalloc(sizeof(Esp32Messaging::TimeVal)));

    if (timeVal == NULL)
    {
        return (nullptr);
    }

    timeVal->TvSec = ExtractUInt32(buffer);
    buffer += 4;
    timeVal->TvUsec = ExtractUInt32(buffer);
    return (timeVal);
}

/**
 *  Extract the SetSockOptRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeSetSockOptRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SetSockOptRequest object.
 *
 *  @returns
 *      Pointer to a SetSockOptRequest object.
 */
Esp32Messaging::SetSockOptRequest *Encoders::ExtractSetSockOptRequest(uint8_t *buffer)
{
    Esp32Messaging::SetSockOptRequest *setSockOptRequest = static_cast<Esp32Messaging::SetSockOptRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SetSockOptRequest)));

    if (setSockOptRequest == NULL)
    {
        return (nullptr);
    }

    setSockOptRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->Level = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionName = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (setSockOptRequest->OptionValueLength > 0)
    {
        setSockOptRequest->OptionValue = static_cast<uint8_t *>(pvPortMalloc(setSockOptRequest->OptionValueLength));
        if (setSockOptRequest->OptionValue == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&setSockOptRequest));
            return (nullptr);
        }
        memcpy(setSockOptRequest->OptionValue, buffer, setSockOptRequest->OptionValueLength);
        buffer += setSockOptRequest->OptionValueLength;
    }
    else
    {
        setSockOptRequest->OptionValue = nullptr;
    }
    setSockOptRequest->OptionLen = ExtractInt32(buffer);
    return (setSockOptRequest);
}

/**
 *  Release the heap storage used by a SetSockOptRequest object along with the storage used by
 *  the nested OptionValue member.
 *
 *  @param setSockOptRequest
 *      Pointer to the pointer to the SetSockOptRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeSetSockOptRequest(Esp32Messaging::SetSockOptRequest **setSockOptRequest)
{
    if ((setSockOptRequest == nullptr) || (*setSockOptRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*setSockOptRequest)->OptionValue));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(setSockOptRequest));
}

/**
 *  Extract the GetSockOptRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockOptRequest object.
 *
 *  @returns
 *      Pointer to a GetSockOptRequest object.
 */
Esp32Messaging::GetSockOptRequest *Encoders::ExtractGetSockOptRequest(uint8_t *buffer)
{
    Esp32Messaging::GetSockOptRequest *getSockOptRequest = static_cast<Esp32Messaging::GetSockOptRequest *>(pvPortMalloc(sizeof(Esp32Messaging::GetSockOptRequest)));

    if (getSockOptRequest == NULL)
    {
        return (nullptr);
    }

    getSockOptRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    getSockOptRequest->Level = ExtractInt32(buffer);
    buffer += 4;
    getSockOptRequest->OptionName = ExtractInt32(buffer);
    return (getSockOptRequest);
}

/**
 *  Convert the GetSockOptResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedGetSockOptResponseBufferSize method.
 *
 *  @param getSockOptResponse
 *      GetSockOptResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockOptResponse object.
 */
void Encoders::EncodeGetSockOptResponse(const Esp32Messaging::GetSockOptResponse * const getSockOptResponse, uint8_t *buffer)
{
    EncodeInt32(getSockOptResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(getSockOptResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(getSockOptResponse->OptionValueLength, buffer);
    buffer += 4;
    if (getSockOptResponse->OptionValueLength > 0)
    {
        memcpy(buffer, getSockOptResponse->OptionValue, getSockOptResponse->OptionValueLength);
        buffer += getSockOptResponse->OptionValueLength;
    }
    EncodeInt32(getSockOptResponse->OptionLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockOptResponse object.
 *
 *  @param getSockOptResponse
 *      GetSockOptResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded GetSockOptResponse object.
 */
int Encoders::EncodedGetSockOptResponseBufferSize(const Esp32Messaging::GetSockOptResponse * const getSockOptResponse)
{
    int result = 0;
    result += getSockOptResponse->OptionValueLength;
    return (result + 16);
}

/**
 *  Convert the Linger object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedLingerBufferSize method.
 *
 *  @param linger
 *      Linger object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded Linger object.
 */
void Encoders::EncodeLinger(const Esp32Messaging::Linger * const linger, uint8_t *buffer)
{
    EncodeInt32(linger->LOnOff, buffer);
    buffer += 4;
    EncodeInt32(linger->LLinger, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the Linger object.
 *
 *  @param linger
 *      Linger object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded Linger object.
 */
int Encoders::EncodedLingerBufferSize(const Esp32Messaging::Linger * const linger)
{
    return (8);
}

/**
 *  Extract the WriteRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeWriteRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded WriteRequest object.
 *
 *  @returns
 *      Pointer to a WriteRequest object.
 */
Esp32Messaging::WriteRequest *Encoders::ExtractWriteRequest(uint8_t *buffer)
{
    Esp32Messaging::WriteRequest *writeRequest = static_cast<Esp32Messaging::WriteRequest *>(pvPortMalloc(sizeof(Esp32Messaging::WriteRequest)));

    if (writeRequest == NULL)
    {
        return (nullptr);
    }

    writeRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    writeRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (writeRequest->BufferLength > 0)
    {
        writeRequest->Buffer = static_cast<uint8_t *>(pvPortMalloc(writeRequest->BufferLength));
        if (writeRequest->Buffer == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&writeRequest));
            return (nullptr);
        }
        memcpy(writeRequest->Buffer, buffer, writeRequest->BufferLength);
        buffer += writeRequest->BufferLength;
    }
    else
    {
        writeRequest->Buffer = nullptr;
    }
    writeRequest->Count = ExtractInt32(buffer);
    return (writeRequest);
}

/**
 *  Release the heap storage used by a WriteRequest object along with the storage used by
 *  the nested Buffer member.
 *
 *  @param writeRequest
 *      Pointer to the pointer to the WriteRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeWriteRequest(Esp32Messaging::WriteRequest **writeRequest)
{
    if ((writeRequest == nullptr) || (*writeRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*writeRequest)->Buffer));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(writeRequest));
}

/**
 *  Extract the ReadRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ReadRequest object.
 *
 *  @returns
 *      Pointer to a ReadRequest object.
 */
Esp32Messaging::ReadRequest *Encoders::ExtractReadRequest(uint8_t *buffer)
{
    Esp32Messaging::ReadRequest *readRequest = static_cast<Esp32Messaging::ReadRequest *>(pvPortMalloc(sizeof(Esp32Messaging::ReadRequest)));

    if (readRequest == NULL)
    {
        return (nullptr);
    }

    readRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    readRequest->Count = ExtractInt32(buffer);
    return (readRequest);
}

/**
 *  Convert the ReadResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedReadResponseBufferSize method.
 *
 *  @param readResponse
 *      ReadResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ReadResponse object.
 */
void Encoders::EncodeReadResponse(const Esp32Messaging::ReadResponse * const readResponse, uint8_t *buffer)
{
    EncodeUInt32(readResponse->BufferLength, buffer);
    buffer += 4;
    if (readResponse->BufferLength > 0)
    {
        memcpy(buffer, readResponse->Buffer, readResponse->BufferLength);
        buffer += readResponse->BufferLength;
    }
    EncodeInt32(readResponse->ReadResponseResult, buffer);
    buffer += 4;
    EncodeInt32(readResponse->ReadResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ReadResponse object.
 *
 *  @param readResponse
 *      ReadResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded ReadResponse object.
 */
int Encoders::EncodedReadResponseBufferSize(const Esp32Messaging::ReadResponse * const readResponse)
{
    int result = 0;
    result += readResponse->BufferLength;
    return (result + 12);
}

/**
 *  Extract the CloseRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded CloseRequest object.
 *
 *  @returns
 *      Pointer to a CloseRequest object.
 */
Esp32Messaging::CloseRequest *Encoders::ExtractCloseRequest(uint8_t *buffer)
{
    Esp32Messaging::CloseRequest *closeRequest = static_cast<Esp32Messaging::CloseRequest *>(pvPortMalloc(sizeof(Esp32Messaging::CloseRequest)));

    if (closeRequest == NULL)
    {
        return (nullptr);
    }

    closeRequest->SocketHandle = ExtractInt32(buffer);
    return (closeRequest);
}

/**
 *  Convert the GetBatteryChargeLevelResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedGetBatteryChargeLevelResponseBufferSize method.
 *
 *  @param getBatteryChargeLevelResponse
 *      GetBatteryChargeLevelResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetBatteryChargeLevelResponse object.
 */
void Encoders::EncodeGetBatteryChargeLevelResponse(const Esp32Messaging::GetBatteryChargeLevelResponse * const getBatteryChargeLevelResponse, uint8_t *buffer)
{
    EncodeUInt32(getBatteryChargeLevelResponse->Level, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetBatteryChargeLevelResponse object.
 *
 *  @param getBatteryChargeLevelResponse
 *      GetBatteryChargeLevelResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded GetBatteryChargeLevelResponse object.
 */
int Encoders::EncodedGetBatteryChargeLevelResponseBufferSize(const Esp32Messaging::GetBatteryChargeLevelResponse * const getBatteryChargeLevelResponse)
{
    return (4);
}

/**
 *  Extract the SendRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeSendRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendRequest object.
 *
 *  @returns
 *      Pointer to a SendRequest object.
 */
Esp32Messaging::SendRequest *Encoders::ExtractSendRequest(uint8_t *buffer)
{
    Esp32Messaging::SendRequest *sendRequest = static_cast<Esp32Messaging::SendRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SendRequest)));

    if (sendRequest == NULL)
    {
        return (nullptr);
    }

    sendRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendRequest->BufferLength > 0)
    {
        sendRequest->Buffer = static_cast<uint8_t *>(pvPortMalloc(sendRequest->BufferLength));
        if (sendRequest->Buffer == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sendRequest));
            return (nullptr);
        }
        memcpy(sendRequest->Buffer, buffer, sendRequest->BufferLength);
        buffer += sendRequest->BufferLength;
    }
    else
    {
        sendRequest->Buffer = nullptr;
    }
    sendRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    sendRequest->Flags = ExtractInt32(buffer);
    return (sendRequest);
}

/**
 *  Release the heap storage used by a SendRequest object along with the storage used by
 *  the nested Buffer member.
 *
 *  @param sendRequest
 *      Pointer to the pointer to the SendRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeSendRequest(Esp32Messaging::SendRequest **sendRequest)
{
    if ((sendRequest == nullptr) || (*sendRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendRequest)->Buffer));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(sendRequest));
}

/**
 *  Extract the SendToRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeSendToRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendToRequest object.
 *
 *  @returns
 *      Pointer to a SendToRequest object.
 */
Esp32Messaging::SendToRequest *Encoders::ExtractSendToRequest(uint8_t *buffer)
{
    Esp32Messaging::SendToRequest *sendToRequest = static_cast<Esp32Messaging::SendToRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SendToRequest)));

    if (sendToRequest == NULL)
    {
        return (nullptr);
    }

    sendToRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendToRequest->BufferLength > 0)
    {
        sendToRequest->Buffer = static_cast<uint8_t *>(pvPortMalloc(sendToRequest->BufferLength));
        if (sendToRequest->Buffer == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sendToRequest));
            return (nullptr);
        }
        memcpy(sendToRequest->Buffer, buffer, sendToRequest->BufferLength);
        buffer += sendToRequest->BufferLength;
    }
    else
    {
        sendToRequest->Buffer = nullptr;
    }
    sendToRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->DestinationAddressLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendToRequest->DestinationAddressLength > 0)
    {
        sendToRequest->DestinationAddress = static_cast<uint8_t *>(pvPortMalloc(sendToRequest->DestinationAddressLength));
        if (sendToRequest->DestinationAddress == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sendToRequest->Buffer));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sendToRequest));
            return (nullptr);
        }
        memcpy(sendToRequest->DestinationAddress, buffer, sendToRequest->DestinationAddressLength);
        buffer += sendToRequest->DestinationAddressLength;
    }
    else
    {
        sendToRequest->DestinationAddress = nullptr;
    }
    return (sendToRequest);
}

/**
 *  Release the heap storage used by a SendToRequest object along with the storage used by
 *  the nested Buffer and DestinationAddress members.
 *
 *  @param sendToRequest
 *      Pointer to the pointer to the SendToRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeSendToRequest(Esp32Messaging::SendToRequest **sendToRequest)
{
    if ((sendToRequest == nullptr) || (*sendToRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendToRequest)->Buffer));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendToRequest)->DestinationAddress));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(sendToRequest));
}

/**
 *  Extract the RecvFromRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvFromRequest object.
 *
 *  @returns
 *      Pointer to a RecvFromRequest object.
 */
Esp32Messaging::RecvFromRequest *Encoders::ExtractRecvFromRequest(uint8_t *buffer)
{
    Esp32Messaging::RecvFromRequest *recvFromRequest = static_cast<Esp32Messaging::RecvFromRequest *>(pvPortMalloc(sizeof(Esp32Messaging::RecvFromRequest)));

    if (recvFromRequest == NULL)
    {
        return (nullptr);
    }

    recvFromRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->GetSourceAddress = ExtractInt32(buffer);
    return (recvFromRequest);
}

/**
 *  Convert the RecvFromResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedRecvFromResponseBufferSize method.
 *
 *  @param recvFromResponse
 *      RecvFromResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded RecvFromResponse object.
 */
void Encoders::EncodeRecvFromResponse(const Esp32Messaging::RecvFromResponse * const recvFromResponse, uint8_t *buffer)
{
    EncodeUInt32(recvFromResponse->BufferLength, buffer);
    buffer += 4;
    if (recvFromResponse->BufferLength > 0)
    {
        memcpy(buffer, recvFromResponse->Buffer, recvFromResponse->BufferLength);
        buffer += recvFromResponse->BufferLength;
    }
    EncodeInt32(recvFromResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(recvFromResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(recvFromResponse->SourceAddressLength, buffer);
    buffer += 4;
    if (recvFromResponse->SourceAddressLength > 0)
    {
        memcpy(buffer, recvFromResponse->SourceAddress, recvFromResponse->SourceAddressLength);
        buffer += recvFromResponse->SourceAddressLength;
    }
    EncodeUInt32(recvFromResponse->SourceAddressLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvFromResponse object.
 *
 *  @param recvFromResponse
 *      RecvFromResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded RecvFromResponse object.
 */
int Encoders::EncodedRecvFromResponseBufferSize(const Esp32Messaging::RecvFromResponse * const recvFromResponse)
{
    int result = 0;
    result += recvFromResponse->BufferLength;
    result += recvFromResponse->SourceAddressLength;
    return (result + 20);
}

/**
 *  Extract the SendMsgRequest object that is encoded in the given buffer.
 *
 *  The outer wire format is:
 *    [4] SocketHandle (int32)
 *    [4] Flags (int32)
 *    [4] MessageHeaderLength (uint32)
 *    [*] MessageHeader (MessageHeaderLength bytes) - the serialised msghdr, laid out as:
 *          [4] NameLength (uint32)
 *          [*] Name (NameLength bytes, if > 0) - raw NuttX sockaddr bytes
 *          [4] IovecCount (uint32)
 *          [4 * IovecCount] Iovec lengths (uint32 each)
 *          [*] Iovec data, concatenated (total length is the sum of the iovec lengths above)
 *          [4] ControlLength (uint32)
 *          [*] Control (ControlLength bytes, if > 0)
 *          [4] MsgFlags (int32)
 *
 *  The individual iovec buffers are not preserved separately; since they are
 *  concatenated on the wire in order, they are extracted here as a single
 *  coalesced Buffer/BufferLength pair.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeSendMsgRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendMsgRequest object.
 *
 *  @returns
 *      Pointer to a SendMsgRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSendMsgRequest
Esp32Messaging::SendMsgRequest *Encoders::ExtractSendMsgRequest(uint8_t *buffer)
{
    if (buffer == nullptr)
    {
        return (nullptr);
    }

    Esp32Messaging::SendMsgRequest *sendMsgRequest = static_cast<Esp32Messaging::SendMsgRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SendMsgRequest)));

    if (sendMsgRequest == NULL)
    {
        return (nullptr);
    }

    sendMsgRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendMsgRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    buffer += 4; // Skip MessageHeaderLength; the nested fields are self-describing.

    sendMsgRequest->NameLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendMsgRequest->NameLength > 0)
    {
        sendMsgRequest->Name = static_cast<uint8_t *>(pvPortMalloc(sendMsgRequest->NameLength));
        if (sendMsgRequest->Name == NULL)
        {
            vPortFree(sendMsgRequest);
            return (nullptr);
        }
        memcpy(sendMsgRequest->Name, buffer, sendMsgRequest->NameLength);
        buffer += sendMsgRequest->NameLength;
    }
    else
    {
        sendMsgRequest->Name = nullptr;
    }

    uint32_t iovecCount = ExtractUInt32(buffer);
    buffer += 4;
    sendMsgRequest->BufferLength = 0;
    for (uint32_t index = 0; index < iovecCount; index++)
    {
        sendMsgRequest->BufferLength += ExtractUInt32(buffer);
        buffer += 4;
    }
    if (sendMsgRequest->BufferLength > 0)
    {
        sendMsgRequest->Buffer = static_cast<uint8_t *>(pvPortMalloc(sendMsgRequest->BufferLength));
        if (sendMsgRequest->Buffer == NULL)
        {
            if (sendMsgRequest->Name != nullptr)
            {
                vPortFree(sendMsgRequest->Name);
            }
            vPortFree(sendMsgRequest);
            return (nullptr);
        }
        memcpy(sendMsgRequest->Buffer, buffer, sendMsgRequest->BufferLength);
        buffer += sendMsgRequest->BufferLength;
    }
    else
    {
        sendMsgRequest->Buffer = nullptr;
    }

    sendMsgRequest->ControlLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendMsgRequest->ControlLength > 0)
    {
        sendMsgRequest->Control = static_cast<uint8_t *>(pvPortMalloc(sendMsgRequest->ControlLength));
        if (sendMsgRequest->Control == NULL)
        {
            if (sendMsgRequest->Name != nullptr)
            {
                vPortFree(sendMsgRequest->Name);
            }
            if (sendMsgRequest->Buffer != nullptr)
            {
                vPortFree(sendMsgRequest->Buffer);
            }
            vPortFree(sendMsgRequest);
            return (nullptr);
        }
        memcpy(sendMsgRequest->Control, buffer, sendMsgRequest->ControlLength);
        buffer += sendMsgRequest->ControlLength;
    }
    else
    {
        sendMsgRequest->Control = nullptr;
    }

    sendMsgRequest->MsgFlags = ExtractInt32(buffer);

    return (sendMsgRequest);
}

/**
 *  Release the heap storage used by a SendMsgRequest object along with the storage used by
 *  the nested Name, Buffer and Control members.
 *
 *  @param sendMsgRequest
 *      Pointer to the pointer to the SendMsgRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeSendMsgRequest(Esp32Messaging::SendMsgRequest **sendMsgRequest)
{
    if ((sendMsgRequest == nullptr) || (*sendMsgRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendMsgRequest)->Name));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendMsgRequest)->Buffer));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*sendMsgRequest)->Control));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(sendMsgRequest));
}

/**
 *  Extract the RecvMsgRequest object that is encoded in the given buffer.
 *
 *  The wire format is:
 *    [4] SocketHandle (int32)
 *    [4] Length (int32)
 *    [4] Flags (int32)
 *    [4] GetSourceAddress (int32)
 *    [4] ControlLength (uint32)
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released by calling vPortFree(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvMsgRequest object.
 *
 *  @returns
 *      Pointer to a RecvMsgRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractRecvMsgRequest
Esp32Messaging::RecvMsgRequest *Encoders::ExtractRecvMsgRequest(uint8_t *buffer)
{
    if (buffer == nullptr)
    {
        return (nullptr);
    }

    Esp32Messaging::RecvMsgRequest *recvMsgRequest = static_cast<Esp32Messaging::RecvMsgRequest *>(pvPortMalloc(sizeof(Esp32Messaging::RecvMsgRequest)));

    if (recvMsgRequest == NULL)
    {
        return (nullptr);
    }

    recvMsgRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    recvMsgRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    recvMsgRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    recvMsgRequest->GetSourceAddress = ExtractInt32(buffer);
    buffer += 4;
    recvMsgRequest->ControlLength = ExtractUInt32(buffer);
    return (recvMsgRequest);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvMsgRequest object.
 *
 *  @param recvMsgRequest
 *      RecvMsgRequest object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded RecvMsgRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedRecvMsgRequestBufferSize
int Encoders::EncodedRecvMsgRequestBufferSize(const Esp32Messaging::RecvMsgRequest * const recvMsgRequest)
{
    return (20);
}

/**
 *  Convert the RecvMsgResponse object into a byte stream that can be sent to the STM32.
 *
 *  The wire format is:
 *    [4] BufferLength (uint32)
 *    [*] Buffer (BufferLength bytes, if > 0)
 *    [4] Result (int32)
 *    [4] ResponseErrno (int32)
 *    [4] SourceAddressLength (uint32)
 *    [*] SourceAddress (SourceAddressLength bytes, if > 0)
 *    [4] SourceAddressLen (uint32)
 *    [4] ControlLength (uint32)
 *    [*] Control (ControlLength bytes, if > 0)
 *    [4] MsgFlags (int32)
 *
 *  The amount of memory required can be obtained by calling the EncodedRecvMsgResponseBufferSize method.
 *
 *  @param recvMsgResponse
 *      RecvMsgResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded RecvMsgResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeRecvMsgResponse
void Encoders::EncodeRecvMsgResponse(const Esp32Messaging::RecvMsgResponse * const recvMsgResponse, uint8_t *buffer)
{
    EncodeUInt32(recvMsgResponse->BufferLength, buffer);
    buffer += 4;
    if (recvMsgResponse->BufferLength > 0)
    {
        memcpy(buffer, recvMsgResponse->Buffer, recvMsgResponse->BufferLength);
        buffer += recvMsgResponse->BufferLength;
    }
    EncodeInt32(recvMsgResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(recvMsgResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(recvMsgResponse->SourceAddressLength, buffer);
    buffer += 4;
    if (recvMsgResponse->SourceAddressLength > 0)
    {
        memcpy(buffer, recvMsgResponse->SourceAddress, recvMsgResponse->SourceAddressLength);
        buffer += recvMsgResponse->SourceAddressLength;
    }
    EncodeUInt32(recvMsgResponse->SourceAddressLen, buffer);
    buffer += 4;
    EncodeUInt32(recvMsgResponse->ControlLength, buffer);
    buffer += 4;
    if (recvMsgResponse->ControlLength > 0)
    {
        memcpy(buffer, recvMsgResponse->Control, recvMsgResponse->ControlLength);
        buffer += recvMsgResponse->ControlLength;
    }
    EncodeInt32(recvMsgResponse->MsgFlags, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvMsgResponse object.
 *
 *  @param recvMsgResponse
 *      RecvMsgResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded RecvMsgResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedRecvMsgResponseBufferSize
int Encoders::EncodedRecvMsgResponseBufferSize(const Esp32Messaging::RecvMsgResponse * const recvMsgResponse)
{
    int result = 0;
    result += recvMsgResponse->BufferLength;
    result += recvMsgResponse->SourceAddressLength;
    result += recvMsgResponse->ControlLength;
    return (result + 28);
}

/**
 *  Extract the PollRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded PollRequest object.
 *
 *  @returns
 *      Pointer to a PollRequest object.
 */
Esp32Messaging::PollRequest *Encoders::ExtractPollRequest(uint8_t *buffer)
{
    Esp32Messaging::PollRequest *pollRequest = static_cast<Esp32Messaging::PollRequest *>(pvPortMalloc(sizeof(Esp32Messaging::PollRequest)));

    if (pollRequest == NULL)
    {
        return (nullptr);
    }

    pollRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->Events = ExtractUInt16(buffer);
    buffer += 2;
    pollRequest->Timeout = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->Setup = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->SetupMessageId = ExtractUInt32(buffer);
    return (pollRequest);
}

/**
 *  Convert the PollResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedPollResponseBufferSize method.
 *
 *  @param pollResponse
 *      PollResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded PollResponse object.
 */
void Encoders::EncodePollResponse(const Esp32Messaging::PollResponse * const pollResponse, uint8_t *buffer)
{
    EncodeUInt16(pollResponse->ReturnedEvents, buffer);
    buffer += 2;
    EncodeInt32(pollResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(pollResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the PollResponse object.
 *
 *  @param pollResponse
 *      PollResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded PollResponse object.
 */
int Encoders::EncodedPollResponseBufferSize(const Esp32Messaging::PollResponse * const pollResponse)
{
    return (10);
}

/**
 *  Convert the InterruptPollResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedInterruptPollResponseBufferSize method.
 *
 *  @param interruptPollResponse
 *      InterruptPollResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded InterruptPollResponse object.
 */
void Encoders::EncodeInterruptPollResponse(const Esp32Messaging::InterruptPollResponse * const interruptPollResponse, uint8_t *buffer)
{
    EncodeInt32(interruptPollResponse->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(interruptPollResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(interruptPollResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt16(interruptPollResponse->ReturnedEvents, buffer);
    buffer += 2;
    EncodeUInt32(interruptPollResponse->SetupMessageId, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the InterruptPollResponse object.
 *
 *  @param interruptPollResponse
 *      InterruptPollResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded InterruptPollResponse object.
 */
int Encoders::EncodedInterruptPollResponseBufferSize(const Esp32Messaging::InterruptPollResponse * const interruptPollResponse)
{
    return (18);
}

/**
 *  Extract the ListenRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ListenRequest object.
 *
 *  @returns
 *      Pointer to a ListenRequest object.
 */
Esp32Messaging::ListenRequest *Encoders::ExtractListenRequest(uint8_t *buffer)
{
    Esp32Messaging::ListenRequest *listenRequest = static_cast<Esp32Messaging::ListenRequest *>(pvPortMalloc(sizeof(Esp32Messaging::ListenRequest)));

    if (listenRequest == NULL)
    {
        return (nullptr);
    }

    listenRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    listenRequest->BackLog = ExtractInt32(buffer);
    return (listenRequest);
}

/**
 *  Extract the BindRequest object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeBindRequest(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BindRequest object.
 *
 *  @returns
 *      Pointer to a BindRequest object.
 */
Esp32Messaging::BindRequest *Encoders::ExtractBindRequest(uint8_t *buffer)
{
    Esp32Messaging::BindRequest *bindRequest = static_cast<Esp32Messaging::BindRequest *>(pvPortMalloc(sizeof(Esp32Messaging::BindRequest)));

    if (bindRequest == NULL)
    {
        return (nullptr);
    }

    bindRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    bindRequest->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bindRequest->AddrLength > 0)
    {
        bindRequest->Addr = static_cast<uint8_t *>(pvPortMalloc(bindRequest->AddrLength));
        if (bindRequest->Addr == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&bindRequest));
            return (nullptr);
        }
        memcpy(bindRequest->Addr, buffer, bindRequest->AddrLength);
        buffer += bindRequest->AddrLength;
    }
    else
    {
        bindRequest->Addr = nullptr;
    }
    return (bindRequest);
}

/**
 *  Release the heap storage used by a BindRequest object along with the storage used by
 *  the nested Addr member.
 *
 *  @param bindRequest
 *      Pointer to the pointer to the BindRequest object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeBindRequest(Esp32Messaging::BindRequest **bindRequest)
{
    if ((bindRequest == nullptr) || (*bindRequest == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*bindRequest)->Addr));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(bindRequest));
}

/**
 *  Extract the AcceptRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AcceptRequest object.
 *
 *  @returns
 *      Pointer to a AcceptRequest object.
 */
Esp32Messaging::AcceptRequest *Encoders::ExtractAcceptRequest(uint8_t *buffer)
{
    Esp32Messaging::AcceptRequest *acceptRequest = static_cast<Esp32Messaging::AcceptRequest *>(pvPortMalloc(sizeof(Esp32Messaging::AcceptRequest)));

    if (acceptRequest == NULL)
    {
        return (nullptr);
    }

    acceptRequest->SocketHandle = ExtractInt32(buffer);
    return (acceptRequest);
}

/**
 *  Convert the AcceptResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedAcceptResponseBufferSize method.
 *
 *  @param acceptResponse
 *      AcceptResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AcceptResponse object.
 */
void Encoders::EncodeAcceptResponse(const Esp32Messaging::AcceptResponse * const acceptResponse, uint8_t *buffer)
{
    EncodeUInt32(acceptResponse->AddrLength, buffer);
    buffer += 4;
    if (acceptResponse->AddrLength > 0)
    {
        memcpy(buffer, acceptResponse->Addr, acceptResponse->AddrLength);
        buffer += acceptResponse->AddrLength;
    }
    EncodeInt32(acceptResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(acceptResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AcceptResponse object.
 *
 *  @param acceptResponse
 *      AcceptResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded AcceptResponse object.
 */
int Encoders::EncodedAcceptResponseBufferSize(const Esp32Messaging::AcceptResponse * const acceptResponse)
{
    int result = 0;
    result += acceptResponse->AddrLength;
    return (result + 12);
}

/**
 *  Extract the IoctlRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IoctlRequest object.
 *
 *  @returns
 *      Pointer to a IoctlRequest object.
 */
Esp32Messaging::IoctlRequest *Encoders::ExtractIoctlRequest(uint8_t *buffer)
{
    Esp32Messaging::IoctlRequest *ioctlRequest = static_cast<Esp32Messaging::IoctlRequest *>(pvPortMalloc(sizeof(Esp32Messaging::IoctlRequest)));

    if (ioctlRequest == NULL)
    {
        return (nullptr);
    }

    ioctlRequest->Command = ExtractInt32(buffer);
    return (ioctlRequest);
}

/**
 *  Convert the IoctlResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedIoctlResponseBufferSize method.
 *
 *  @param ioctlResponse
 *      IoctlResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IoctlResponse object.
 */
void Encoders::EncodeIoctlResponse(const Esp32Messaging::IoctlResponse * const ioctlResponse, uint8_t *buffer)
{
    EncodeUInt32(ioctlResponse->AddrLength, buffer);
    buffer += 4;
    if (ioctlResponse->AddrLength > 0)
    {
        memcpy(buffer, ioctlResponse->Addr, ioctlResponse->AddrLength);
        buffer += ioctlResponse->AddrLength;
    }
    EncodeInt32(ioctlResponse->Flags, buffer);
    buffer += 4;
    EncodeInt32(ioctlResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(ioctlResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IoctlResponse object.
 *
 *  @param ioctlResponse
 *      IoctlResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded IoctlResponse object.
 */
int Encoders::EncodedIoctlResponseBufferSize(const Esp32Messaging::IoctlResponse * const ioctlResponse)
{
    int result = 0;
    result += ioctlResponse->AddrLength;
    return (result + 16);
}

/**
 *  Extract the GetSockPeerNameRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockPeerNameRequest object.
 *
 *  @returns
 *      Pointer to a GetSockPeerNameRequest object.
 */
Esp32Messaging::GetSockPeerNameRequest *Encoders::ExtractGetSockPeerNameRequest(uint8_t *buffer)
{
    Esp32Messaging::GetSockPeerNameRequest *getSockPeerNameRequest = static_cast<Esp32Messaging::GetSockPeerNameRequest *>(pvPortMalloc(sizeof(Esp32Messaging::GetSockPeerNameRequest)));

    if (getSockPeerNameRequest == NULL)
    {
        return (nullptr);
    }

    getSockPeerNameRequest->SocketHandle = ExtractInt32(buffer);
    return (getSockPeerNameRequest);
}

/**
 *  Convert the GetSockPeerNameResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedGetSockPeerNameResponseBufferSize method.
 *
 *  @param getSockPeerNameResponse
 *      GetSockPeerNameResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockPeerNameResponse object.
 */
void Encoders::EncodeGetSockPeerNameResponse(const Esp32Messaging::GetSockPeerNameResponse * const getSockPeerNameResponse, uint8_t *buffer)
{
    EncodeUInt32(getSockPeerNameResponse->AddrLength, buffer);
    buffer += 4;
    if (getSockPeerNameResponse->AddrLength > 0)
    {
        memcpy(buffer, getSockPeerNameResponse->Addr, getSockPeerNameResponse->AddrLength);
        buffer += getSockPeerNameResponse->AddrLength;
    }
    EncodeInt32(getSockPeerNameResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(getSockPeerNameResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockPeerNameResponse object.
 *
 *  @param getSockPeerNameResponse
 *      GetSockPeerNameResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded GetSockPeerNameResponse object.
 */
int Encoders::EncodedGetSockPeerNameResponseBufferSize(const Esp32Messaging::GetSockPeerNameResponse * const getSockPeerNameResponse)
{
    int result = 0;
    result += getSockPeerNameResponse->AddrLength;
    return (result + 12);
}

/**
 *  Convert the EventData object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedEventDataBufferSize method.
 *
 *  @param eventData
 *      EventData object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded EventData object.
 */
void Encoders::EncodeEventData(const Esp32Messaging::EventData * const eventData, uint8_t *buffer)
{
    *buffer = eventData->Interface;
    buffer += 1;
    EncodeUInt32(eventData->Function, buffer);
    buffer += 4;
    EncodeUInt32(eventData->StatusCode, buffer);
    buffer += 4;
    EncodeUInt32(eventData->MessageId, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the EventData object.
 *
 *  @param eventData
 *      EventData object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded EventData object.
 */
int Encoders::EncodedEventDataBufferSize(const Esp32Messaging::EventData * const eventData)
{
    return (13);
}

/**
 *  Convert the EventDataPayload object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedEventDataPayloadBufferSize method.
 *
 *  @param eventDataPayload
 *      EventDataPayload object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded EventDataPayload object.
 */
void Encoders::EncodeEventDataPayload(const Esp32Messaging::EventDataPayload * const eventDataPayload, uint8_t *buffer)
{
    EncodeUInt32(eventDataPayload->MessageId, buffer);
    buffer += 4;
    EncodeUInt32(eventDataPayload->PayloadLength, buffer);
    buffer += 4;
    if (eventDataPayload->PayloadLength > 0)
    {
        memcpy(buffer, eventDataPayload->Payload, eventDataPayload->PayloadLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the EventDataPayload object.
 *
 *  @param eventDataPayload
 *      EventDataPayload object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded EventDataPayload object.
 */
int Encoders::EncodedEventDataPayloadBufferSize(const Esp32Messaging::EventDataPayload * const eventDataPayload)
{
    int result = 0;
    result += eventDataPayload->PayloadLength;
    return (result + 8);
}

/**
 *  Extract the SetAntennaRequest object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SetAntennaRequest object.
 *
 *  @returns
 *      Pointer to a SetAntennaRequest object.
 */
Esp32Messaging::SetAntennaRequest *Encoders::ExtractSetAntennaRequest(uint8_t *buffer)
{
    Esp32Messaging::SetAntennaRequest *setAntennaRequest = static_cast<Esp32Messaging::SetAntennaRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SetAntennaRequest)));

    if (setAntennaRequest == NULL)
    {
        return (nullptr);
    }

    setAntennaRequest->Antenna = *buffer;
    buffer += 1;
    setAntennaRequest->Persist = *buffer;
    return (setAntennaRequest);
}

/**
 *  Extract the BTStackConfig object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeBTStackConfig(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTStackConfig object.
 *
 *  @returns
 *      Pointer to a BTStackConfig object.
 */
Esp32Messaging::BTStackConfig *Encoders::ExtractBTStackConfig(uint8_t *buffer)
{
    Esp32Messaging::BTStackConfig *bTStackConfig = static_cast<Esp32Messaging::BTStackConfig *>(pvPortMalloc(sizeof(Esp32Messaging::BTStackConfig)));

    if (bTStackConfig == NULL)
    {
        return (nullptr);
    }

    bTStackConfig->Config = ExtractString(buffer);
    return (bTStackConfig);
}

/**
 *  Release the heap storage used by a BTStackConfig object along with the storage used by
 *  the nested Config member.
 *
 *  @param bTStackConfig
 *      Pointer to the pointer to the BTStackConfig object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeBTStackConfig(Esp32Messaging::BTStackConfig **bTStackConfig)
{
    if ((bTStackConfig == nullptr) || (*bTStackConfig == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*bTStackConfig)->Config));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(bTStackConfig));
}

/**
 *  Convert the BTDataWriteRequest object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedBTDataWriteRequestBufferSize method.
 *
 *  @param bTDataWriteRequest
 *      BTDataWriteRequest object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTDataWriteRequest object.
 */
void Encoders::EncodeBTDataWriteRequest(const Esp32Messaging::BTDataWriteRequest * const bTDataWriteRequest, uint8_t *buffer)
{
    EncodeUInt16(bTDataWriteRequest->Handle, buffer);
    buffer += 2;
    EncodeUInt32(bTDataWriteRequest->DataLength, buffer);
    buffer += 4;
    if (bTDataWriteRequest->DataLength > 0)
    {
        memcpy(buffer, bTDataWriteRequest->Data, bTDataWriteRequest->DataLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTDataWriteRequest object.
 *
 *  @param bTDataWriteRequest
 *      BTDataWriteRequest object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded BTDataWriteRequest object.
 */
int Encoders::EncodedBTDataWriteRequestBufferSize(const Esp32Messaging::BTDataWriteRequest * const bTDataWriteRequest)
{
    int result = 0;
    result += bTDataWriteRequest->DataLength;
    return (result + 6);
}

/**
 *  Convert the BTGetHandlesResponse object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedBTGetHandlesResponseBufferSize method.
 *
 *  @param bTGetHandlesResponse
 *      BTGetHandlesResponse object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTGetHandlesResponse object.
 */
void Encoders::EncodeBTGetHandlesResponse(const Esp32Messaging::BTGetHandlesResponse * const bTGetHandlesResponse, uint8_t *buffer)
{
    EncodeUInt16(bTGetHandlesResponse->HandleCount, buffer);
    buffer += 2;
    EncodeUInt32(bTGetHandlesResponse->HandlesLength, buffer);
    buffer += 4;
    if (bTGetHandlesResponse->HandlesLength > 0)
    {
        memcpy(buffer, bTGetHandlesResponse->Handles, bTGetHandlesResponse->HandlesLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTGetHandlesResponse object.
 *
 *  @param bTGetHandlesResponse
 *      BTGetHandlesResponse object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded BTGetHandlesResponse object.
 */
int Encoders::EncodedBTGetHandlesResponseBufferSize(const Esp32Messaging::BTGetHandlesResponse * const bTGetHandlesResponse)
{
    int result = 0;
    result += bTGetHandlesResponse->HandlesLength;
    return (result + 6);
}

/**
 *  Extract the BTServerDataSet object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeBTServerDataSet(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTServerDataSet object.
 *
 *  @returns
 *      Pointer to a BTServerDataSet object.
 */
Esp32Messaging::BTServerDataSet *Encoders::ExtractBTServerDataSet(uint8_t *buffer)
{
    Esp32Messaging::BTServerDataSet *bTServerDataSet = static_cast<Esp32Messaging::BTServerDataSet *>(pvPortMalloc(sizeof(Esp32Messaging::BTServerDataSet)));

    if (bTServerDataSet == NULL)
    {
        return (nullptr);
    }

    bTServerDataSet->Handle = ExtractUInt16(buffer);
    buffer += 2;
    bTServerDataSet->SetDataLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bTServerDataSet->SetDataLength > 0)
    {
        bTServerDataSet->SetData = static_cast<uint8_t *>(pvPortMalloc(bTServerDataSet->SetDataLength));
        if (bTServerDataSet->SetData == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&bTServerDataSet));
            return (nullptr);
        }
        memcpy(bTServerDataSet->SetData, buffer, bTServerDataSet->SetDataLength);
        buffer += bTServerDataSet->SetDataLength;
    }
    else
    {
        bTServerDataSet->SetData = nullptr;
    }
    return (bTServerDataSet);
}

/**
 *  Release the heap storage used by a BTServerDataSet object along with the storage used by
 *  the nested SetData member.
 *
 *  @param bTServerDataSet
 *      Pointer to the pointer to the BTServerDataSet object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeBTServerDataSet(Esp32Messaging::BTServerDataSet **bTServerDataSet)
{
    if ((bTServerDataSet == nullptr) || (*bTServerDataSet == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*bTServerDataSet)->SetData));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(bTServerDataSet));
}

/**
 *  Convert the FileDetails object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedFileDetailsBufferSize method.
 *
 *  @param fileDetails
 *      FileDetails object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileDetails object.
 */
void Encoders::EncodeFileDetails(const Esp32Messaging::FileDetails * const fileDetails, uint8_t *buffer)
{
    EncodeString(fileDetails->Name, buffer);
    buffer += StringLength(fileDetails->Name) + 1;
    EncodeUInt16(fileDetails->Length, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileDetails object.
 *
 *  @param fileDetails
 *      FileDetails object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded FileDetails object.
 */
int Encoders::EncodedFileDetailsBufferSize(const Esp32Messaging::FileDetails * const fileDetails)
{
    int result = 0;
    result += Encoders::StringLength(fileDetails->Name);
    return (result + 3);
}

/**
 *  Extract the FileDetails object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeFileDetails(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileDetails object.
 *
 *  @returns
 *      Pointer to a FileDetails object.
 */
Esp32Messaging::FileDetails *Encoders::ExtractFileDetails(uint8_t *buffer)
{
    Esp32Messaging::FileDetails *fileDetails = static_cast<Esp32Messaging::FileDetails *>(pvPortMalloc(sizeof(Esp32Messaging::FileDetails)));

    if (fileDetails == NULL)
    {
        return (nullptr);
    }

    fileDetails->Name = ExtractString(buffer);
    buffer += Encoders::StringLength(fileDetails->Name) + 1;
    fileDetails->Length = ExtractUInt16(buffer);
    return (fileDetails);
}

/**
 *  Release the heap storage used by a FileDetails object along with the storage used by
 *  the nested Name member.
 *
 *  @param fileDetails
 *      Pointer to the pointer to the FileDetails object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeFileDetails(Esp32Messaging::FileDetails **fileDetails)
{
    if ((fileDetails == nullptr) || (*fileDetails == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*fileDetails)->Name));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(fileDetails));
}

/**
 *  Convert the FileNameAndContents object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedFileNameAndContentsBufferSize method.
 *
 *  @param fileNameAndContents
 *      FileNameAndContents object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileNameAndContents object.
 */
void Encoders::EncodeFileNameAndContents(const Esp32Messaging::FileNameAndContents * const fileNameAndContents, uint8_t *buffer)
{
    EncodeString(fileNameAndContents->Name, buffer);
    buffer += StringLength(fileNameAndContents->Name) + 1;
    EncodeUInt32(fileNameAndContents->ContentsLength, buffer);
    buffer += 4;
    if (fileNameAndContents->ContentsLength > 0)
    {
        memcpy(buffer, fileNameAndContents->Contents, fileNameAndContents->ContentsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileNameAndContents object.
 *
 *  @param fileNameAndContents
 *      FileNameAndContents object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded FileNameAndContents object.
 */
int Encoders::EncodedFileNameAndContentsBufferSize(const Esp32Messaging::FileNameAndContents * const fileNameAndContents)
{
    int result = 0;
    result += Encoders::StringLength(fileNameAndContents->Name);
    result += fileNameAndContents->ContentsLength;
    return (result + 5);
}

/**
 *  Extract the FileNameAndContents object that is encoded in the given buffer.
 *
 *  Note that the returned object holds nested allocations, so it should eventually be
 *  released by calling Encoders::FreeFileNameAndContents(...) rather than by freeing the object
 *  on its own.
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileNameAndContents object.
 *
 *  @returns
 *      Pointer to a FileNameAndContents object.
 */
Esp32Messaging::FileNameAndContents *Encoders::ExtractFileNameAndContents(uint8_t *buffer)
{
    Esp32Messaging::FileNameAndContents *fileNameAndContents = static_cast<Esp32Messaging::FileNameAndContents *>(pvPortMalloc(sizeof(Esp32Messaging::FileNameAndContents)));

    if (fileNameAndContents == NULL)
    {
        return (nullptr);
    }

    fileNameAndContents->Name = ExtractString(buffer);
    buffer += Encoders::StringLength(fileNameAndContents->Name) + 1;
    fileNameAndContents->ContentsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (fileNameAndContents->ContentsLength > 0)
    {
        fileNameAndContents->Contents = static_cast<uint8_t *>(pvPortMalloc(fileNameAndContents->ContentsLength));
        if (fileNameAndContents->Contents == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileNameAndContents));
            return (nullptr);
        }
        memcpy(fileNameAndContents->Contents, buffer, fileNameAndContents->ContentsLength);
        buffer += fileNameAndContents->ContentsLength;
    }
    else
    {
        fileNameAndContents->Contents = nullptr;
    }
    return (fileNameAndContents);
}

/**
 *  Release the heap storage used by a FileNameAndContents object along with the storage used by
 *  the nested Name and Contents members.
 *
 *  @param fileNameAndContents
 *      Pointer to the pointer to the FileNameAndContents object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeFileNameAndContents(Esp32Messaging::FileNameAndContents **fileNameAndContents)
{
    if ((fileNameAndContents == nullptr) || (*fileNameAndContents == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*fileNameAndContents)->Name));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*fileNameAndContents)->Contents));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(fileNameAndContents));
}

/**
 *  Convert the FileNameList object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedFileNameListBufferSize method.
 *
 *  @param fileNameList
 *      FileNameList object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileNameList object.
 */
void Encoders::EncodeFileNameList(const Esp32Messaging::FileNameList * const fileNameList, uint8_t *buffer)
{
    EncodeUInt16(fileNameList->NumberOfFiles, buffer);
    buffer += 2;
    EncodeUInt32(fileNameList->FileDetailsLength, buffer);
    buffer += 4;
    if (fileNameList->FileDetailsLength > 0)
    {
        memcpy(buffer, fileNameList->FileDetails, fileNameList->FileDetailsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileNameList object.
 *
 *  @param fileNameList
 *      FileNameList object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded FileNameList object.
 */
int Encoders::EncodedFileNameListBufferSize(const Esp32Messaging::FileNameList * const fileNameList)
{
    int result = 0;
    result += fileNameList->FileDetailsLength;
    return (result + 6);
}

/**
 * @brief Extract the logging configuration from the given buffer.
 *
 * @note The returned object holds nested allocations, so it should eventually be released by
 *       calling Encoders::FreeLoggingConfiguration(...) rather than by freeing the object on its own.
 *
 * @param buffer Block of memory holding the encoded logging configuration.
 * @return Esp32Messaging::LoggingConfiguration* Pointer to the extracted logging configuration.
 */
Esp32Messaging::LoggingConfiguration *Encoders::ExtractLoggingConfiguration(uint8_t *buffer)
{
    Esp32Messaging::LoggingConfiguration *loggingConfiguration = static_cast<Esp32Messaging::LoggingConfiguration *>(pvPortMalloc(sizeof(Esp32Messaging::LoggingConfiguration)));

    if (loggingConfiguration == NULL)
    {
        return (nullptr);
    }

    loggingConfiguration->Interface = *buffer;
    buffer++;
    loggingConfiguration->UdpPort = ExtractUInt16(buffer);
    buffer += 2;
    if (*buffer == 0)
    {
        loggingConfiguration->ComponentsToLog = nullptr;
    }
    else
    {
        loggingConfiguration->ComponentsToLog = ExtractString(buffer);
    }
    return (loggingConfiguration);
}

/**
 *  Release the heap storage used by a LoggingConfiguration object along with the storage used by
 *  the nested ComponentsToLog member.
 *
 *  @param loggingConfiguration
 *      Pointer to the pointer to the LoggingConfiguration object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeLoggingConfiguration(Esp32Messaging::LoggingConfiguration **loggingConfiguration)
{
    if ((loggingConfiguration == nullptr) || (*loggingConfiguration == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*loggingConfiguration)->ComponentsToLog));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(loggingConfiguration));
}

/**
 * @brief Extract the log message and level from the given buffer.
 *
 * @note The returned object holds nested allocations, so it should eventually be released by
 *       calling Encoders::FreeLogMessage(...) rather than by freeing the object on its own.
 *
 * @param buffer Block of memory holding the encoded log message.
 * @return Esp32Messaging::LogMessage* Pointer to the extracted log message.
 */
Esp32Messaging::LogMessage *Encoders::ExtractLogMessage(uint8_t *buffer)
{
    Esp32Messaging::LogMessage *logMessage = static_cast<Esp32Messaging::LogMessage *>(pvPortMalloc(sizeof(Esp32Messaging::LogMessage)));

    if (logMessage == NULL)
    {
        return (nullptr);
    }

    logMessage->Level = *buffer;
    buffer++;
    if (*buffer == 0)
    {
        logMessage->Message = nullptr;
    }
    else
    {
        logMessage->Message = ExtractString(buffer);
    }
    return (logMessage);
}

/**
 *  Release the heap storage used by a LogMessage object along with the storage used by
 *  the nested Message member.
 *
 *  @param logMessage
 *      Pointer to the pointer to the LogMessage object to be released.  The pointer is
 *      set to nullptr when the object has been released.
 */
void Encoders::FreeLogMessage(Esp32Messaging::LogMessage **logMessage)
{
    if ((logMessage == nullptr) || (*logMessage == nullptr))
    {
        return;
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&(*logMessage)->Message));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(logMessage));
}

/**
 *  Convert the GotIpEventData object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedGotIpEventDataBufferSize method.
 *
 *  @param gotIpEventData
 *      GotIpEventData object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded EventData object.
 */
void Encoders::EncodeGotIpEventData(const Esp32Messaging::GotIpEventData * const gotIpEventData, uint8_t *buffer)
{
    EncodeUInt32(gotIpEventData->DnsAddress, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GotIpEventData object.
 *
 *  @param gotIpEventData
 *      GotIpEventData object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded GotIpEventData object.
 */
int Encoders::EncodedGotIpEventDataBufferSize(const Esp32Messaging::GotIpEventData * const gotIpEventData)
{
    return (4);
}

/**
 * @brief Encode the CoreDumpInformation into the provided buffer.
 *
 * @param coreDumpInfo Pointer to the CoreDumpInformation.
 * @param buffer Pointer to the buffer to write the encoded data.
 */
void Encoders::EncodeCoreDumpInformation(const Esp32Messaging::CoreDumpInformationResponse * const coreDumpInfo, uint8_t *buffer)
{
    if (coreDumpInfo == NULL || buffer == NULL)
    {
        return;
    }

    *buffer = coreDumpInfo->IsValid;
    buffer++;
    EncodeUInt32(coreDumpInfo->CoreDumpSize, buffer);
    buffer += 4;
    EncodeUInt32(coreDumpInfo->PartitionSize, buffer);
}

/**
 * @brief Get the size of the encoded CoreDumpInformation.
 *
 * @param coreDumpInfo Pointer to the CoreDumpInformation.
 * @return int Size of the encoded CoreDumpInformation.
 */
int Encoders::EncodedCoreDumpInformationBufferSize(const Esp32Messaging::CoreDumpInformationResponse * const coreDumpInfo)
{
    if (coreDumpInfo == NULL)
    {
        return -1;
    }

    return (9);
}

/**
 * @brief Extract a core dump fragment request from the given buffer.
 *
 * @param buffer Block of memory holding the encoded core dump fragment request.
 * @return Esp32Messaging::CoreDumpFragmentRequest* Pointer to the extracted core dump fragment request.
 */
Esp32Messaging::CoreDumpFragmentRequest *Encoders::ExtractCoreDumpFragmentRequest(uint8_t *buffer)
{
    if (buffer == NULL)
    {
        return (nullptr);
    }

    Esp32Messaging::CoreDumpFragmentRequest *fragmentRequest = static_cast<Esp32Messaging::CoreDumpFragmentRequest *>(pvPortMalloc(sizeof(Esp32Messaging::CoreDumpFragmentRequest)));
    if (fragmentRequest == NULL)
    {
        return (nullptr);
    }

    fragmentRequest->Offset = ExtractUInt32(buffer);
    buffer += 4;
    fragmentRequest->Size = ExtractUInt32(buffer);

    return (fragmentRequest);
}

/**
 * @brief Encode the CoreDumpFragment into the provided buffer.
 *
 * @param fragment Pointer to the CoreDumpFragment.
 * @param buffer Pointer to the buffer to write the encoded data.
 */
void Encoders::EncodeCoreDumpFragmentResponse(const Esp32Messaging::CoreDumpFragmentResponse * const fragment, uint8_t *buffer)
{
    if (fragment == NULL || buffer == NULL)
    {
        return;
    }

    EncodeUInt32(fragment->Size, buffer);
    buffer += 4;
    memcpy(buffer, fragment->Data, fragment->Size);
}

/**
 * @brief Get the size of the encoded CoreDumpFragment.
 *
 * @param fragment Pointer to the CoreDumpFragment.
 * @return int Size of the encoded CoreDumpFragment.
 */
int Encoders::EncodedCoreDumpFragmentResponseBufferSize(const Esp32Messaging::CoreDumpFragmentResponse * const fragment)
{
    if (fragment == NULL)
    {
        return -1;
    }

    return (8 + fragment->Size);
}