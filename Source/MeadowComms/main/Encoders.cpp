
#include "Encoders.hpp"

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
uint32_t Encoders::CalculateSpiBufferSize(uint32_t requestedSize)
{
    uint32_t result = requestedSize;

    if (result < 8)
    {
        result = 8;
    }
    //
    //  The buffer should always be 4 bytes longer than needed.  During development it
    //  was found that the last four bytes of any transmission were being discarded.
    //  Empirical tests proved this for 24, 32 and 40 byte packets.
    //
    //  The work around is to increase the packet size by 4 and have dummy data in the
    //  last four bytes and discard the bytes.
    //
    //
    //  See support post: https://esp32.com/viewtopic.php?f=13&t=10117
    //
    requestedSize += 4;
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    else
    {
        result += 4;
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
// cppcheck-suppress unusedFunction symbolName=ExtractUInt16
uint16_t Encoders::ExtractUInt16(uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodeUInt16
void Encoders::EncodeUInt16(uint16_t value, uint8_t *buffer)
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
uint32_t Encoders::ExtractUInt32(uint8_t *buffer)
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
void Encoders::EncodeUInt32(uint32_t value, uint8_t *buffer)
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
uint32_t Encoders::ExtractInt32(uint8_t *buffer)
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
void Encoders::EncodeInt32(int32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Extract a string (terminated by 0) from a block of memory.
 *
 *  @param buffer
 *      Pointer to the block of memory containing the string.
 *
 *  @returns
 *      Pointer to the extracted string.
 */
char *Encoders::ExtractString(uint8_t *buffer)
{
    uint32_t length = 0;
    uint8_t *ptr = buffer;

    while (*ptr != 0)
    {
        ptr++;
        length++;
    }

    ptr = (uint8_t *) pvPortMalloc(length + 1);
    strcpy((char *) ptr, (char *) buffer);
    return((char *) ptr);
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
    strcpy((char *) buffer, (char *) source);
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
// cppcheck-suppress unusedFunction symbolName=CRC8
uint8_t Encoders::CRC8(const uint8_t *data, uint16_t len)
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
    return(crc);
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
uint32_t Encoders::CRC32(const uint8_t *data, uint16_t len)
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
uint32_t Encoders::ProgressiveCRC32(uint32_t currentChecksum, uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        uint32_t mask = (uint32_t) -(crc & 0x01);
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}

/**
 *  Extract a message from a block of memory and create a new Message object.
 *
 *  @param buffer
 *      Byte (uint8_t) array containing the encoded message.
 *
 *  @param bufferLength
 *      Size of the buffer.
 *
 *  @param header
 *      Indicate if we should extract the header information only
 *      (i.e. do not copy the payload).
 *
 *  @returns
 *      New Message object extracted from the buffer.  The Message object
 *     Will be placed on the FreeRTOS heap (use vPortFree to release the
 *     storage).
 */
Message *Encoders::ExtractMessage(uint8_t *buffer, uint32_t bufferLength, bool headerOnly)
{
    uint32_t crc = ExtractUInt32(buffer + Message::CRC_OFFSET);
    EncodeUInt32(0, buffer + Message::CRC_OFFSET);
    
    Message *message = nullptr;
    if (crc == CRC32(buffer, bufferLength))
    {
        message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));

        memset((void *) message, 0, sizeof(Message));
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
        //
        //  Move over the last field and skip the CRC entry.
        //
        buffer += 8;
        if (!headerOnly && (message->PayloadLength > 0))
        {
            message->Payload = (uint8_t *) pvPortMalloc(message->PayloadLength);
            memcpy(message->Payload, buffer, message->PayloadLength);
        }
        else
        {
            message->Payload = nullptr;
        }
    }
    else
    {
        return (nullptr);
    }
    return(message);
}

/**
 *  Encode a message into a block of memory.
 *
 *  @param message
 *      Message structure to encode in to the byte buffer.
 *
 *  @param header
 *      Copy this as a header only (i.e. do not copy the payload).
 *
 *  @returns
 *      byte buffer containing the encoded message.  The storage
 *     Will be placed on the FreeRTOS heap (use vPortFree to release the
 *     storage).
 */
uint8_t *Encoders::EncodeMessage(Message *message, bool headerOnly)
{
    uint32_t messageSize = Message::HEADER_SIZE;
    if (!headerOnly)
    {
        messageSize += message->PayloadLength;
    }
    uint32_t bufferSize = Encoders::CalculateSpiBufferSize(messageSize);
    int offset = 0;
    uint8_t *buffer = (uint8_t *) pvPortMalloc(bufferSize);
    
    memset(buffer, 0, bufferSize);
    *buffer = message->MessageType;
    offset++;
    *(buffer + offset) = message->Interface;
    offset++;
    EncodeUInt32(message->Function, buffer + offset);
    offset += 4;
    EncodeUInt32(message->StatusCode, buffer + offset);
    offset += 4;
    EncodeUInt32(message->MessageID, buffer + offset);
    offset += 4;
    EncodeUInt32(message->PayloadLength, buffer + offset);
    offset += 4;
    //
    //  The byte after the PayloadLength is the CRC.  This is filled in later
    //  and so a 0 is written into the message temporarily.
    //
    EncodeUInt32(0, buffer + offset);
    offset += 4;
    if (!headerOnly && (message->PayloadLength > 0))
    {
        memcpy((uint8_t *) (buffer + offset), message->Payload, message->PayloadLength);
    }
    else
    {
        EncodeUInt32(0, buffer + offset);
    }
    uint32_t crc = CRC32(buffer, bufferSize);
    EncodeUInt32(crc, buffer + Message::CRC_OFFSET);

    return(buffer);
}


/*
 *******************************************************************************

       THE METHODS BELOW HAVE BEEN AUTOMATICALLY GENERATED BASED UPON THE
       Messages.json METADATA FILE.

 *******************************************************************************
*/

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
// cppcheck-suppress unusedFunction symbolName=EncodeSystemConfiguration
void Encoders::EncodeSystemConfiguration(Esp32Messaging::SystemConfiguration *systemConfiguration, uint8_t *buffer)
{
    EncodeUInt32(systemConfiguration->MessageSize, buffer);
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
// cppcheck-suppress unusedFunction symbolName=EncodedSystemConfigurationBufferSize
int Encoders::EncodedSystemConfigurationBufferSize(Esp32Messaging::SystemConfiguration *systemConfiguration)
{
    return(4);
}

/**
 *  Extract the SystemConfiguration object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SystemConfiguration object.
 *  
 *  @returns
 *      Pointer to a SystemConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSystemConfiguration
Esp32Messaging::SystemConfiguration *Encoders::ExtractSystemConfiguration(uint8_t *buffer)
{
    Esp32Messaging::SystemConfiguration *systemConfiguration = (Esp32Messaging::SystemConfiguration *) pvPortMalloc(sizeof(Esp32Messaging::SystemConfiguration));

    systemConfiguration->MessageSize = ExtractUInt32(buffer);
    return(systemConfiguration);
}

/**
 *  Convert the WiFiConfiguration object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedWiFiConfigurationBufferSize method.
 *  
 *  @param wiFiConfiguration
 *      WiFiConfiguration object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded WiFiConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeWiFiConfiguration
void Encoders::EncodeWiFiConfiguration(Esp32Messaging::WiFiConfiguration *wiFiConfiguration, uint8_t *buffer)
{
    *buffer = wiFiConfiguration->AutomaticReconnect;
    buffer += 1;
    EncodeUInt32(wiFiConfiguration->MaximumRetryCount, buffer);
    buffer += 4;
    *buffer = wiFiConfiguration->Antenna;
    buffer += 1;
    *buffer = wiFiConfiguration->MaximumMessageQueueLength;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the WiFiConfiguration object.
 *  
 *  @param wiFiConfiguration
 *      WiFiConfiguration object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded WiFiConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedWiFiConfigurationBufferSize
int Encoders::EncodedWiFiConfigurationBufferSize(Esp32Messaging::WiFiConfiguration *wiFiConfiguration)
{
    return(7);
}

/**
 *  Extract the WiFiConfiguration object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded WiFiConfiguration object.
 *  
 *  @returns
 *      Pointer to a WiFiConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractWiFiConfiguration
Esp32Messaging::WiFiConfiguration *Encoders::ExtractWiFiConfiguration(uint8_t *buffer)
{
    Esp32Messaging::WiFiConfiguration *wiFiConfiguration = (Esp32Messaging::WiFiConfiguration *) pvPortMalloc(sizeof(Esp32Messaging::WiFiConfiguration));

    wiFiConfiguration->AutomaticReconnect = *buffer;
    buffer += 1;
    wiFiConfiguration->MaximumRetryCount = ExtractUInt32(buffer);
    buffer += 4;
    wiFiConfiguration->Antenna = *buffer;
    buffer += 1;
    wiFiConfiguration->MaximumMessageQueueLength = *buffer;
    return(wiFiConfiguration);
}

/**
 *  Convert the WiFiCredentials object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedWiFiCredentialsBufferSize method.
 *  
 *  @param wiFiCredentials
 *      WiFiCredentials object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded WiFiCredentials object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeWiFiCredentials
void Encoders::EncodeWiFiCredentials(Esp32Messaging::WiFiCredentials *wiFiCredentials, uint8_t *buffer)
{
    EncodeString(wiFiCredentials->NetworkName, buffer);
    buffer += strlen(wiFiCredentials->NetworkName) + 1;
    EncodeString(wiFiCredentials->Password, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the WiFiCredentials object.
 *  
 *  @param wiFiCredentials
 *      WiFiCredentials object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded WiFiCredentials object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedWiFiCredentialsBufferSize
int Encoders::EncodedWiFiCredentialsBufferSize(Esp32Messaging::WiFiCredentials *wiFiCredentials)
{
    int result = 0;
    result += strlen(wiFiCredentials->NetworkName);
    result += strlen(wiFiCredentials->Password);
    return(result + 2);
}

/**
 *  Extract the WiFiCredentials object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded WiFiCredentials object.
 *  
 *  @returns
 *      Pointer to a WiFiCredentials object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractWiFiCredentials
Esp32Messaging::WiFiCredentials *Encoders::ExtractWiFiCredentials(uint8_t *buffer)
{
    Esp32Messaging::WiFiCredentials *wiFiCredentials = (Esp32Messaging::WiFiCredentials *) pvPortMalloc(sizeof(Esp32Messaging::WiFiCredentials));

    wiFiCredentials->NetworkName = ExtractString(buffer);
    buffer += strlen(wiFiCredentials->NetworkName) + 1;
    wiFiCredentials->Password = ExtractString(buffer);
    return(wiFiCredentials);
}

/**
 *  Convert the AntennaInfo object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAntennaInfoBufferSize method.
 *  
 *  @param antennaInfo
 *      AntennaInfo object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AntennaInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAntennaInfo
void Encoders::EncodeAntennaInfo(Esp32Messaging::AntennaInfo *antennaInfo, uint8_t *buffer)
{
    *buffer = antennaInfo->Antenna;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AntennaInfo object.
 *  
 *  @param antennaInfo
 *      AntennaInfo object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AntennaInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAntennaInfoBufferSize
int Encoders::EncodedAntennaInfoBufferSize(Esp32Messaging::AntennaInfo *antennaInfo)
{
    return(1);
}

/**
 *  Extract the AntennaInfo object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AntennaInfo object.
 *  
 *  @returns
 *      Pointer to a AntennaInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAntennaInfo
Esp32Messaging::AntennaInfo *Encoders::ExtractAntennaInfo(uint8_t *buffer)
{
    Esp32Messaging::AntennaInfo *antennaInfo = (Esp32Messaging::AntennaInfo *) pvPortMalloc(sizeof(Esp32Messaging::AntennaInfo));

    antennaInfo->Antenna = *buffer;
    return(antennaInfo);
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
// cppcheck-suppress unusedFunction symbolName=EncodeAccessPoint
void Encoders::EncodeAccessPoint(Esp32Messaging::AccessPoint *accessPoint, uint8_t *buffer)
{
    memcpy((void *) buffer, (void *) accessPoint->Ssid, 33);
    buffer += 33;
    memcpy((void *) buffer, (void *) accessPoint->Bssid, 6);
    buffer += 6;
    *buffer = accessPoint->PrimaryChannel;
    buffer += 1;
    *buffer = accessPoint->SecondaryChannel;
    buffer += 1;
    *buffer = (uint8_t) accessPoint->Rssi;
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
// cppcheck-suppress unusedFunction symbolName=EncodedAccessPointBufferSize
int Encoders::EncodedAccessPointBufferSize(Esp32Messaging::AccessPoint *accessPoint)
{
    return(47);
}

/**
 *  Extract the AccessPoint object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPoint object.
 *  
 *  @returns
 *      Pointer to a AccessPoint object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAccessPoint
Esp32Messaging::AccessPoint *Encoders::ExtractAccessPoint(uint8_t *buffer)
{
    Esp32Messaging::AccessPoint *accessPoint = (Esp32Messaging::AccessPoint *) pvPortMalloc(sizeof(Esp32Messaging::AccessPoint));

    memcpy((void *) accessPoint->Ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) accessPoint->Bssid, (void *) buffer, 6);
    buffer += 6;
    accessPoint->PrimaryChannel = *buffer;
    buffer += 1;
    accessPoint->SecondaryChannel = *buffer;
    buffer += 1;
    accessPoint->Rssi = (int8_t) *buffer;
    buffer += 1;
    accessPoint->AuthenticationMode = *buffer;
    buffer += 1;
    accessPoint->Protocols = ExtractUInt32(buffer);
    return(accessPoint);
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
// cppcheck-suppress unusedFunction symbolName=EncodeAccessPointList
void Encoders::EncodeAccessPointList(Esp32Messaging::AccessPointList *accessPointList, uint8_t *buffer)
{
    EncodeUInt32(accessPointList->NumberOfAccessPoints, buffer);
    buffer += 4;
    EncodeUInt32(accessPointList->AccessPointsLength, buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        memcpy((void *) buffer, (void *) accessPointList->AccessPoints, accessPointList->AccessPointsLength);
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
// cppcheck-suppress unusedFunction symbolName=EncodedAccessPointListBufferSize
int Encoders::EncodedAccessPointListBufferSize(Esp32Messaging::AccessPointList *accessPointList)
{
    int result = 0;
    result += accessPointList->AccessPointsLength;
    return(result + 8);
}

/**
 *  Extract the AccessPointList object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointList object.
 *  
 *  @returns
 *      Pointer to a AccessPointList object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAccessPointList
Esp32Messaging::AccessPointList *Encoders::ExtractAccessPointList(uint8_t *buffer)
{
    Esp32Messaging::AccessPointList *accessPointList = (Esp32Messaging::AccessPointList *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointList));

    accessPointList->NumberOfAccessPoints = ExtractUInt32(buffer);
    buffer += 4;
    accessPointList->AccessPointsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        accessPointList->AccessPoints = (uint8_t *) pvPortMalloc(accessPointList->AccessPointsLength);
        memcpy(accessPointList->AccessPoints, buffer, accessPointList->AccessPointsLength);
        buffer += accessPointList->AccessPointsLength;
    }
    else
    {
        accessPointList->AccessPoints = nullptr;
    }
    return(accessPointList);
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
// cppcheck-suppress unusedFunction symbolName=EncodeSockAddr
void Encoders::EncodeSockAddr(Esp32Messaging::SockAddr *sockAddr, uint8_t *buffer)
{
    *buffer = sockAddr->Length;
    buffer += 1;
    *buffer = sockAddr->Family;
    buffer += 1;
    memcpy((void *) buffer, (void *) sockAddr->Data, 14);
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
// cppcheck-suppress unusedFunction symbolName=EncodedSockAddrBufferSize
int Encoders::EncodedSockAddrBufferSize(Esp32Messaging::SockAddr *sockAddr)
{
    return(16);
}

/**
 *  Extract the SockAddr object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SockAddr object.
 *  
 *  @returns
 *      Pointer to a SockAddr object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSockAddr
Esp32Messaging::SockAddr *Encoders::ExtractSockAddr(uint8_t *buffer)
{
    Esp32Messaging::SockAddr *sockAddr = (Esp32Messaging::SockAddr *) pvPortMalloc(sizeof(Esp32Messaging::SockAddr));

    sockAddr->Length = *buffer;
    buffer += 1;
    sockAddr->Family = *buffer;
    buffer += 1;
    memcpy((void *) sockAddr->Data, (void *) buffer, 14);
    return(sockAddr);
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
// cppcheck-suppress unusedFunction symbolName=EncodeAddrInfo
void Encoders::EncodeAddrInfo(Esp32Messaging::AddrInfo *addrInfo, uint8_t *buffer)
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
        memcpy((void *) buffer, (void *) addrInfo->Addr, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    EncodeString(addrInfo->CanonName, buffer);
    buffer += strlen(addrInfo->CanonName) + 1;
    EncodeUInt32((uint32_t) addrInfo->Next, buffer);
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
// cppcheck-suppress unusedFunction symbolName=EncodedAddrInfoBufferSize
int Encoders::EncodedAddrInfoBufferSize(Esp32Messaging::AddrInfo *addrInfo)
{
    int result = 0;
    result += addrInfo->AddrLength;
    result += strlen(addrInfo->CanonName);
    return(result + 33);
}

/**
 *  Extract the AddrInfo object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AddrInfo object.
 *  
 *  @returns
 *      Pointer to a AddrInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAddrInfo
Esp32Messaging::AddrInfo *Encoders::ExtractAddrInfo(uint8_t *buffer)
{
    Esp32Messaging::AddrInfo *addrInfo = (Esp32Messaging::AddrInfo *) pvPortMalloc(sizeof(Esp32Messaging::AddrInfo));

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
        addrInfo->Addr = (uint8_t *) pvPortMalloc(addrInfo->AddrLength);
        memcpy(addrInfo->Addr, buffer, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    else
    {
        addrInfo->Addr = nullptr;
    }
    addrInfo->CanonName = ExtractString(buffer);
    buffer += strlen(addrInfo->CanonName) + 1;
    addrInfo->Next = (void *) ExtractUInt32(buffer);
    return(addrInfo);
}

/**
 *  Convert the GetAddrInfoRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetAddrInfoRequestBufferSize method.
 *  
 *  @param getAddrInfoRequest
 *      GetAddrInfoRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetAddrInfoRequest
void Encoders::EncodeGetAddrInfoRequest(Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest, uint8_t *buffer)
{
    EncodeString(getAddrInfoRequest->NodeName, buffer);
    buffer += strlen(getAddrInfoRequest->NodeName) + 1;
    EncodeString(getAddrInfoRequest->ServName, buffer);
    buffer += strlen(getAddrInfoRequest->ServName) + 1;
    EncodeUInt32(getAddrInfoRequest->HintsLength, buffer);
    buffer += 4;
    if (getAddrInfoRequest->HintsLength > 0)
    {
        memcpy((void *) buffer, (void *) getAddrInfoRequest->Hints, getAddrInfoRequest->HintsLength);
        buffer += getAddrInfoRequest->HintsLength;
    }
    EncodeUInt32(getAddrInfoRequest->ResultLength, buffer);
    buffer += 4;
    if (getAddrInfoRequest->ResultLength > 0)
    {
        memcpy((void *) buffer, (void *) getAddrInfoRequest->Result, getAddrInfoRequest->ResultLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetAddrInfoRequest object.
 *  
 *  @param getAddrInfoRequest
 *      GetAddrInfoRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetAddrInfoRequestBufferSize
int Encoders::EncodedGetAddrInfoRequestBufferSize(Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest)
{
    int result = 0;
    result += strlen(getAddrInfoRequest->NodeName);
    result += strlen(getAddrInfoRequest->ServName);
    result += getAddrInfoRequest->HintsLength;
    result += getAddrInfoRequest->ResultLength;
    return(result + 10);
}

/**
 *  Extract the GetAddrInfoRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetAddrInfoRequest object.
 *  
 *  @returns
 *      Pointer to a GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetAddrInfoRequest
Esp32Messaging::GetAddrInfoRequest *Encoders::ExtractGetAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest = (Esp32Messaging::GetAddrInfoRequest *) pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoRequest));

    getAddrInfoRequest->NodeName = ExtractString(buffer);
    buffer += strlen(getAddrInfoRequest->NodeName) + 1;
    getAddrInfoRequest->ServName = ExtractString(buffer);
    buffer += strlen(getAddrInfoRequest->ServName) + 1;
    getAddrInfoRequest->HintsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoRequest->HintsLength > 0)
    {
        getAddrInfoRequest->Hints = (uint8_t *) pvPortMalloc(getAddrInfoRequest->HintsLength);
        memcpy(getAddrInfoRequest->Hints, buffer, getAddrInfoRequest->HintsLength);
        buffer += getAddrInfoRequest->HintsLength;
    }
    else
    {
        getAddrInfoRequest->Hints = nullptr;
    }
    getAddrInfoRequest->ResultLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoRequest->ResultLength > 0)
    {
        getAddrInfoRequest->Result = (uint8_t *) pvPortMalloc(getAddrInfoRequest->ResultLength);
        memcpy(getAddrInfoRequest->Result, buffer, getAddrInfoRequest->ResultLength);
        buffer += getAddrInfoRequest->ResultLength;
    }
    else
    {
        getAddrInfoRequest->Result = nullptr;
    }
    return(getAddrInfoRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeGetAddrInfoResponse
void Encoders::EncodeGetAddrInfoResponse(Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse, uint8_t *buffer)
{
    EncodeInt32(getAddrInfoResponse->AddrInfoResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(getAddrInfoResponse->ResLength, buffer);
    buffer += 4;
    if (getAddrInfoResponse->ResLength > 0)
    {
        memcpy((void *) buffer, (void *) getAddrInfoResponse->Res, getAddrInfoResponse->ResLength);
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
// cppcheck-suppress unusedFunction symbolName=EncodedGetAddrInfoResponseBufferSize
int Encoders::EncodedGetAddrInfoResponseBufferSize(Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse)
{
    int result = 0;
    result += getAddrInfoResponse->ResLength;
    return(result + 8);
}

/**
 *  Extract the GetAddrInfoResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetAddrInfoResponse object.
 *  
 *  @returns
 *      Pointer to a GetAddrInfoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetAddrInfoResponse
Esp32Messaging::GetAddrInfoResponse *Encoders::ExtractGetAddrInfoResponse(uint8_t *buffer)
{
    Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse = (Esp32Messaging::GetAddrInfoResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse));

    getAddrInfoResponse->AddrInfoResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    getAddrInfoResponse->ResLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoResponse->ResLength > 0)
    {
        getAddrInfoResponse->Res = (uint8_t *) pvPortMalloc(getAddrInfoResponse->ResLength);
        memcpy(getAddrInfoResponse->Res, buffer, getAddrInfoResponse->ResLength);
        buffer += getAddrInfoResponse->ResLength;
    }
    else
    {
        getAddrInfoResponse->Res = nullptr;
    }
    return(getAddrInfoResponse);
}

/**
 *  Convert the SocketRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSocketRequestBufferSize method.
 *  
 *  @param socketRequest
 *      SocketRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSocketRequest
void Encoders::EncodeSocketRequest(Esp32Messaging::SocketRequest *socketRequest, uint8_t *buffer)
{
    EncodeUInt32((uint32_t) socketRequest->AddressInformation, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Domain, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Type, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Protocol, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SocketRequest object.
 *  
 *  @param socketRequest
 *      SocketRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSocketRequestBufferSize
int Encoders::EncodedSocketRequestBufferSize(Esp32Messaging::SocketRequest *socketRequest)
{
    return(16);
}

/**
 *  Extract the SocketRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SocketRequest object.
 *  
 *  @returns
 *      Pointer to a SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSocketRequest
Esp32Messaging::SocketRequest *Encoders::ExtractSocketRequest(uint8_t *buffer)
{
    Esp32Messaging::SocketRequest *socketRequest = (Esp32Messaging::SocketRequest *) pvPortMalloc(sizeof(Esp32Messaging::SocketRequest));

    socketRequest->AddressInformation = (void *) ExtractUInt32(buffer);
    buffer += 4;
    socketRequest->Domain = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Type = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Protocol = ExtractInt32(buffer);
    return(socketRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeIntegerResponse
void Encoders::EncodeIntegerResponse(Esp32Messaging::IntegerResponse *integerResponse, uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodedIntegerResponseBufferSize
int Encoders::EncodedIntegerResponseBufferSize(Esp32Messaging::IntegerResponse *integerResponse)
{
    return(4);
}

/**
 *  Extract the IntegerResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IntegerResponse object.
 *  
 *  @returns
 *      Pointer to a IntegerResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIntegerResponse
Esp32Messaging::IntegerResponse *Encoders::ExtractIntegerResponse(uint8_t *buffer)
{
    Esp32Messaging::IntegerResponse *integerResponse = (Esp32Messaging::IntegerResponse *) pvPortMalloc(sizeof(Esp32Messaging::IntegerResponse));

    integerResponse->Result = ExtractInt32(buffer);
    return(integerResponse);
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
// cppcheck-suppress unusedFunction symbolName=EncodeIntegerAndErrnoResponse
void Encoders::EncodeIntegerAndErrnoResponse(Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse, uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodedIntegerAndErrnoResponseBufferSize
int Encoders::EncodedIntegerAndErrnoResponseBufferSize(Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse)
{
    return(8);
}

/**
 *  Extract the IntegerAndErrnoResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IntegerAndErrnoResponse object.
 *  
 *  @returns
 *      Pointer to a IntegerAndErrnoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIntegerAndErrnoResponse
Esp32Messaging::IntegerAndErrnoResponse *Encoders::ExtractIntegerAndErrnoResponse(uint8_t *buffer)
{
    Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse = (Esp32Messaging::IntegerAndErrnoResponse *) pvPortMalloc(sizeof(Esp32Messaging::IntegerAndErrnoResponse));

    integerAndErrnoResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    integerAndErrnoResponse->ResponseErrno = ExtractInt32(buffer);
    return(integerAndErrnoResponse);
}

/**
 *  Convert the ConnectRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedConnectRequestBufferSize method.
 *  
 *  @param connectRequest
 *      ConnectRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeConnectRequest
void Encoders::EncodeConnectRequest(Esp32Messaging::ConnectRequest *connectRequest, uint8_t *buffer)
{
    EncodeInt32(connectRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(connectRequest->AddrLength, buffer);
    buffer += 4;
    if (connectRequest->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) connectRequest->Addr, connectRequest->AddrLength);
        buffer += connectRequest->AddrLength;
    }
    EncodeInt32(connectRequest->AddrLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ConnectRequest object.
 *  
 *  @param connectRequest
 *      ConnectRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedConnectRequestBufferSize
int Encoders::EncodedConnectRequestBufferSize(Esp32Messaging::ConnectRequest *connectRequest)
{
    int result = 0;
    result += connectRequest->AddrLength;
    return(result + 12);
}

/**
 *  Extract the ConnectRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConnectRequest object.
 *  
 *  @returns
 *      Pointer to a ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractConnectRequest
Esp32Messaging::ConnectRequest *Encoders::ExtractConnectRequest(uint8_t *buffer)
{
    Esp32Messaging::ConnectRequest *connectRequest = (Esp32Messaging::ConnectRequest *) pvPortMalloc(sizeof(Esp32Messaging::ConnectRequest));

    connectRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    connectRequest->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (connectRequest->AddrLength > 0)
    {
        connectRequest->Addr = (uint8_t *) pvPortMalloc(connectRequest->AddrLength);
        memcpy(connectRequest->Addr, buffer, connectRequest->AddrLength);
        buffer += connectRequest->AddrLength;
    }
    else
    {
        connectRequest->Addr = nullptr;
    }
    connectRequest->AddrLen = ExtractInt32(buffer);
    return(connectRequest);
}

/**
 *  Convert the FreeAddrInfoRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedFreeAddrInfoRequestBufferSize method.
 *  
 *  @param freeAddrInfoRequest
 *      FreeAddrInfoRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeFreeAddrInfoRequest
void Encoders::EncodeFreeAddrInfoRequest(Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest, uint8_t *buffer)
{
    EncodeUInt32(freeAddrInfoRequest->AddrInfoAddress, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FreeAddrInfoRequest object.
 *  
 *  @param freeAddrInfoRequest
 *      FreeAddrInfoRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedFreeAddrInfoRequestBufferSize
int Encoders::EncodedFreeAddrInfoRequestBufferSize(Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest)
{
    return(4);
}

/**
 *  Extract the FreeAddrInfoRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FreeAddrInfoRequest object.
 *  
 *  @returns
 *      Pointer to a FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractFreeAddrInfoRequest
Esp32Messaging::FreeAddrInfoRequest *Encoders::ExtractFreeAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest = (Esp32Messaging::FreeAddrInfoRequest *) pvPortMalloc(sizeof(Esp32Messaging::FreeAddrInfoRequest));

    freeAddrInfoRequest->AddrInfoAddress = ExtractUInt32(buffer);
    return(freeAddrInfoRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeTimeVal
void Encoders::EncodeTimeVal(Esp32Messaging::TimeVal *timeVal, uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodedTimeValBufferSize
int Encoders::EncodedTimeValBufferSize(Esp32Messaging::TimeVal *timeVal)
{
    return(8);
}

/**
 *  Extract the TimeVal object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded TimeVal object.
 *  
 *  @returns
 *      Pointer to a TimeVal object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractTimeVal
Esp32Messaging::TimeVal *Encoders::ExtractTimeVal(uint8_t *buffer)
{
    Esp32Messaging::TimeVal *timeVal = (Esp32Messaging::TimeVal *) pvPortMalloc(sizeof(Esp32Messaging::TimeVal));

    timeVal->TvSec = ExtractUInt32(buffer);
    buffer += 4;
    timeVal->TvUsec = ExtractUInt32(buffer);
    return(timeVal);
}

/**
 *  Convert the SetSockOptRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSetSockOptRequestBufferSize method.
 *  
 *  @param setSockOptRequest
 *      SetSockOptRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSetSockOptRequest
void Encoders::EncodeSetSockOptRequest(Esp32Messaging::SetSockOptRequest *setSockOptRequest, uint8_t *buffer)
{
    EncodeInt32(setSockOptRequest->SocketNumber, buffer);
    buffer += 4;
    EncodeInt32(setSockOptRequest->Level, buffer);
    buffer += 4;
    EncodeInt32(setSockOptRequest->OptionName, buffer);
    buffer += 4;
    EncodeUInt32(setSockOptRequest->OptionValueLength, buffer);
    buffer += 4;
    if (setSockOptRequest->OptionValueLength > 0)
    {
        memcpy((void *) buffer, (void *) setSockOptRequest->OptionValue, setSockOptRequest->OptionValueLength);
        buffer += setSockOptRequest->OptionValueLength;
    }
    EncodeInt32(setSockOptRequest->OptionLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SetSockOptRequest object.
 *  
 *  @param setSockOptRequest
 *      SetSockOptRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSetSockOptRequestBufferSize
int Encoders::EncodedSetSockOptRequestBufferSize(Esp32Messaging::SetSockOptRequest *setSockOptRequest)
{
    int result = 0;
    result += setSockOptRequest->OptionValueLength;
    return(result + 20);
}

/**
 *  Extract the SetSockOptRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SetSockOptRequest object.
 *  
 *  @returns
 *      Pointer to a SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSetSockOptRequest
Esp32Messaging::SetSockOptRequest *Encoders::ExtractSetSockOptRequest(uint8_t *buffer)
{
    Esp32Messaging::SetSockOptRequest *setSockOptRequest = (Esp32Messaging::SetSockOptRequest *) pvPortMalloc(sizeof(Esp32Messaging::SetSockOptRequest));

    setSockOptRequest->SocketNumber = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->Level = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionName = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (setSockOptRequest->OptionValueLength > 0)
    {
        setSockOptRequest->OptionValue = (uint8_t *) pvPortMalloc(setSockOptRequest->OptionValueLength);
        memcpy(setSockOptRequest->OptionValue, buffer, setSockOptRequest->OptionValueLength);
        buffer += setSockOptRequest->OptionValueLength;
    }
    else
    {
        setSockOptRequest->OptionValue = nullptr;
    }
    setSockOptRequest->OptionLen = ExtractInt32(buffer);
    return(setSockOptRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeLinger
void Encoders::EncodeLinger(Esp32Messaging::Linger *linger, uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodedLingerBufferSize
int Encoders::EncodedLingerBufferSize(Esp32Messaging::Linger *linger)
{
    return(8);
}

/**
 *  Extract the Linger object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded Linger object.
 *  
 *  @returns
 *      Pointer to a Linger object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractLinger
Esp32Messaging::Linger *Encoders::ExtractLinger(uint8_t *buffer)
{
    Esp32Messaging::Linger *linger = (Esp32Messaging::Linger *) pvPortMalloc(sizeof(Esp32Messaging::Linger));

    linger->LOnOff = ExtractInt32(buffer);
    buffer += 4;
    linger->LLinger = ExtractInt32(buffer);
    return(linger);
}

/**
 *  Convert the WriteRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedWriteRequestBufferSize method.
 *  
 *  @param writeRequest
 *      WriteRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeWriteRequest
void Encoders::EncodeWriteRequest(Esp32Messaging::WriteRequest *writeRequest, uint8_t *buffer)
{
    EncodeInt32(writeRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(writeRequest->BufferLength, buffer);
    buffer += 4;
    if (writeRequest->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) writeRequest->Buffer, writeRequest->BufferLength);
        buffer += writeRequest->BufferLength;
    }
    EncodeInt32(writeRequest->Count, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the WriteRequest object.
 *  
 *  @param writeRequest
 *      WriteRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedWriteRequestBufferSize
int Encoders::EncodedWriteRequestBufferSize(Esp32Messaging::WriteRequest *writeRequest)
{
    int result = 0;
    result += writeRequest->BufferLength;
    return(result + 12);
}

/**
 *  Extract the WriteRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded WriteRequest object.
 *  
 *  @returns
 *      Pointer to a WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractWriteRequest
Esp32Messaging::WriteRequest *Encoders::ExtractWriteRequest(uint8_t *buffer)
{
    Esp32Messaging::WriteRequest *writeRequest = (Esp32Messaging::WriteRequest *) pvPortMalloc(sizeof(Esp32Messaging::WriteRequest));

    writeRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    writeRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (writeRequest->BufferLength > 0)
    {
        writeRequest->Buffer = (uint8_t *) pvPortMalloc(writeRequest->BufferLength);
        memcpy(writeRequest->Buffer, buffer, writeRequest->BufferLength);
        buffer += writeRequest->BufferLength;
    }
    else
    {
        writeRequest->Buffer = nullptr;
    }
    writeRequest->Count = ExtractInt32(buffer);
    return(writeRequest);
}

/**
 *  Convert the ReadRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedReadRequestBufferSize method.
 *  
 *  @param readRequest
 *      ReadRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeReadRequest
void Encoders::EncodeReadRequest(Esp32Messaging::ReadRequest *readRequest, uint8_t *buffer)
{
    EncodeInt32(readRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(readRequest->Count, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ReadRequest object.
 *  
 *  @param readRequest
 *      ReadRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedReadRequestBufferSize
int Encoders::EncodedReadRequestBufferSize(Esp32Messaging::ReadRequest *readRequest)
{
    return(8);
}

/**
 *  Extract the ReadRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ReadRequest object.
 *  
 *  @returns
 *      Pointer to a ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractReadRequest
Esp32Messaging::ReadRequest *Encoders::ExtractReadRequest(uint8_t *buffer)
{
    Esp32Messaging::ReadRequest *readRequest = (Esp32Messaging::ReadRequest *) pvPortMalloc(sizeof(Esp32Messaging::ReadRequest));

    readRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    readRequest->Count = ExtractInt32(buffer);
    return(readRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeReadResponse
void Encoders::EncodeReadResponse(Esp32Messaging::ReadResponse *readResponse, uint8_t *buffer)
{
    EncodeUInt32(readResponse->BufferLength, buffer);
    buffer += 4;
    if (readResponse->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) readResponse->Buffer, readResponse->BufferLength);
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
// cppcheck-suppress unusedFunction symbolName=EncodedReadResponseBufferSize
int Encoders::EncodedReadResponseBufferSize(Esp32Messaging::ReadResponse *readResponse)
{
    int result = 0;
    result += readResponse->BufferLength;
    return(result + 12);
}

/**
 *  Extract the ReadResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ReadResponse object.
 *  
 *  @returns
 *      Pointer to a ReadResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractReadResponse
Esp32Messaging::ReadResponse *Encoders::ExtractReadResponse(uint8_t *buffer)
{
    Esp32Messaging::ReadResponse *readResponse = (Esp32Messaging::ReadResponse *) pvPortMalloc(sizeof(Esp32Messaging::ReadResponse));

    readResponse->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (readResponse->BufferLength > 0)
    {
        readResponse->Buffer = (uint8_t *) pvPortMalloc(readResponse->BufferLength);
        memcpy(readResponse->Buffer, buffer, readResponse->BufferLength);
        buffer += readResponse->BufferLength;
    }
    else
    {
        readResponse->Buffer = nullptr;
    }
    readResponse->ReadResponseResult = ExtractInt32(buffer);
    buffer += 4;
    readResponse->ReadResponseErrno = ExtractInt32(buffer);
    return(readResponse);
}

/**
 *  Convert the CloseRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedCloseRequestBufferSize method.
 *  
 *  @param closeRequest
 *      CloseRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeCloseRequest
void Encoders::EncodeCloseRequest(Esp32Messaging::CloseRequest *closeRequest, uint8_t *buffer)
{
    EncodeInt32(closeRequest->SocketHandle, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the CloseRequest object.
 *  
 *  @param closeRequest
 *      CloseRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedCloseRequestBufferSize
int Encoders::EncodedCloseRequestBufferSize(Esp32Messaging::CloseRequest *closeRequest)
{
    return(4);
}

/**
 *  Extract the CloseRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded CloseRequest object.
 *  
 *  @returns
 *      Pointer to a CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractCloseRequest
Esp32Messaging::CloseRequest *Encoders::ExtractCloseRequest(uint8_t *buffer)
{
    Esp32Messaging::CloseRequest *closeRequest = (Esp32Messaging::CloseRequest *) pvPortMalloc(sizeof(Esp32Messaging::CloseRequest));

    closeRequest->SocketHandle = ExtractInt32(buffer);
    return(closeRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeGetBatteryChargeLevelResponse
void Encoders::EncodeGetBatteryChargeLevelResponse(Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse, uint8_t *buffer)
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
// cppcheck-suppress unusedFunction symbolName=EncodedGetBatteryChargeLevelResponseBufferSize
int Encoders::EncodedGetBatteryChargeLevelResponseBufferSize(Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse)
{
    return(4);
}

/**
 *  Extract the GetBatteryChargeLevelResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetBatteryChargeLevelResponse object.
 *  
 *  @returns
 *      Pointer to a GetBatteryChargeLevelResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetBatteryChargeLevelResponse
Esp32Messaging::GetBatteryChargeLevelResponse *Encoders::ExtractGetBatteryChargeLevelResponse(uint8_t *buffer)
{
    Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse = (Esp32Messaging::GetBatteryChargeLevelResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetBatteryChargeLevelResponse));

    getBatteryChargeLevelResponse->Level = ExtractUInt32(buffer);
    return(getBatteryChargeLevelResponse);
}

/**
 *  Convert the SendToRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSendToRequestBufferSize method.
 *  
 *  @param sendToRequest
 *      SendToRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSendToRequest
void Encoders::EncodeSendToRequest(Esp32Messaging::SendToRequest *sendToRequest, uint8_t *buffer)
{
    EncodeInt32(sendToRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(sendToRequest->BufferLength, buffer);
    buffer += 4;
    if (sendToRequest->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) sendToRequest->Buffer, sendToRequest->BufferLength);
        buffer += sendToRequest->BufferLength;
    }
    EncodeInt32(sendToRequest->Length, buffer);
    buffer += 4;
    EncodeInt32(sendToRequest->Flags, buffer);
    buffer += 4;
    EncodeUInt32(sendToRequest->DestinationAddressLength, buffer);
    buffer += 4;
    if (sendToRequest->DestinationAddressLength > 0)
    {
        memcpy((void *) buffer, (void *) sendToRequest->DestinationAddress, sendToRequest->DestinationAddressLength);
        buffer += sendToRequest->DestinationAddressLength;
    }
    EncodeInt32(sendToRequest->DestinationAddressLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SendToRequest object.
 *  
 *  @param sendToRequest
 *      SendToRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSendToRequestBufferSize
int Encoders::EncodedSendToRequestBufferSize(Esp32Messaging::SendToRequest *sendToRequest)
{
    int result = 0;
    result += sendToRequest->BufferLength;
    result += sendToRequest->DestinationAddressLength;
    return(result + 24);
}

/**
 *  Extract the SendToRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendToRequest object.
 *  
 *  @returns
 *      Pointer to a SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSendToRequest
Esp32Messaging::SendToRequest *Encoders::ExtractSendToRequest(uint8_t *buffer)
{
    Esp32Messaging::SendToRequest *sendToRequest = (Esp32Messaging::SendToRequest *) pvPortMalloc(sizeof(Esp32Messaging::SendToRequest));

    sendToRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendToRequest->BufferLength > 0)
    {
        sendToRequest->Buffer = (uint8_t *) pvPortMalloc(sendToRequest->BufferLength);
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
        sendToRequest->DestinationAddress = (uint8_t *) pvPortMalloc(sendToRequest->DestinationAddressLength);
        memcpy(sendToRequest->DestinationAddress, buffer, sendToRequest->DestinationAddressLength);
        buffer += sendToRequest->DestinationAddressLength;
    }
    else
    {
        sendToRequest->DestinationAddress = nullptr;
    }
    sendToRequest->DestinationAddressLen = ExtractInt32(buffer);
    return(sendToRequest);
}

/**
 *  Convert the RecvFromRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedRecvFromRequestBufferSize method.
 *  
 *  @param recvFromRequest
 *      RecvFromRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeRecvFromRequest
void Encoders::EncodeRecvFromRequest(Esp32Messaging::RecvFromRequest *recvFromRequest, uint8_t *buffer)
{
    EncodeInt32(recvFromRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->Length, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->Flags, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->GetSourceAddress, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvFromRequest object.
 *  
 *  @param recvFromRequest
 *      RecvFromRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedRecvFromRequestBufferSize
int Encoders::EncodedRecvFromRequestBufferSize(Esp32Messaging::RecvFromRequest *recvFromRequest)
{
    return(16);
}

/**
 *  Extract the RecvFromRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvFromRequest object.
 *  
 *  @returns
 *      Pointer to a RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractRecvFromRequest
Esp32Messaging::RecvFromRequest *Encoders::ExtractRecvFromRequest(uint8_t *buffer)
{
    Esp32Messaging::RecvFromRequest *recvFromRequest = (Esp32Messaging::RecvFromRequest *) pvPortMalloc(sizeof(Esp32Messaging::RecvFromRequest));

    recvFromRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->GetSourceAddress = ExtractInt32(buffer);
    return(recvFromRequest);
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
// cppcheck-suppress unusedFunction symbolName=EncodeRecvFromResponse
void Encoders::EncodeRecvFromResponse(Esp32Messaging::RecvFromResponse *recvFromResponse, uint8_t *buffer)
{
    EncodeUInt32(recvFromResponse->BufferLength, buffer);
    buffer += 4;
    if (recvFromResponse->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) recvFromResponse->Buffer, recvFromResponse->BufferLength);
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
        memcpy((void *) buffer, (void *) recvFromResponse->SourceAddress, recvFromResponse->SourceAddressLength);
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
// cppcheck-suppress unusedFunction symbolName=EncodedRecvFromResponseBufferSize
int Encoders::EncodedRecvFromResponseBufferSize(Esp32Messaging::RecvFromResponse *recvFromResponse)
{
    int result = 0;
    result += recvFromResponse->BufferLength;
    result += recvFromResponse->SourceAddressLength;
    return(result + 20);
}

/**
 *  Extract the RecvFromResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvFromResponse object.
 *  
 *  @returns
 *      Pointer to a RecvFromResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractRecvFromResponse
Esp32Messaging::RecvFromResponse *Encoders::ExtractRecvFromResponse(uint8_t *buffer)
{
    Esp32Messaging::RecvFromResponse *recvFromResponse = (Esp32Messaging::RecvFromResponse *) pvPortMalloc(sizeof(Esp32Messaging::RecvFromResponse));

    recvFromResponse->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (recvFromResponse->BufferLength > 0)
    {
        recvFromResponse->Buffer = (uint8_t *) pvPortMalloc(recvFromResponse->BufferLength);
        memcpy(recvFromResponse->Buffer, buffer, recvFromResponse->BufferLength);
        buffer += recvFromResponse->BufferLength;
    }
    else
    {
        recvFromResponse->Buffer = nullptr;
    }
    recvFromResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    recvFromResponse->ResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    recvFromResponse->SourceAddressLength = ExtractUInt32(buffer);
    buffer += 4;
    if (recvFromResponse->SourceAddressLength > 0)
    {
        recvFromResponse->SourceAddress = (uint8_t *) pvPortMalloc(recvFromResponse->SourceAddressLength);
        memcpy(recvFromResponse->SourceAddress, buffer, recvFromResponse->SourceAddressLength);
        buffer += recvFromResponse->SourceAddressLength;
    }
    else
    {
        recvFromResponse->SourceAddress = nullptr;
    }
    recvFromResponse->SourceAddressLen = ExtractUInt32(buffer);
    return(recvFromResponse);
}


