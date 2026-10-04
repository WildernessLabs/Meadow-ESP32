/*
 *  TestEncodersMessage.cpp
 *
 *  Tests that EncodeMessage writes only inside the buffer it is given, that ExtractMessage reads only
 *  inside the buffer it is given and leaves it as it was, and that both refuse what does not fit
 *  (issue #267).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "unity.h"

#include "Encoders.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Encoders][Emulated]"

/**
 * @brief Number of payload bytes the messages in these tests carry.
 */
#define PAYLOAD_LENGTH 40

/**
 * @brief Value the buffers are filled with, so that a byte the encoder touched can be told from one it did not.
 */
#define SENTINEL 0x5a

/**
 * @brief Create a message with a payload of recognisable bytes, in one packet.
 *
 * @return The message, which the caller releases.
 */
static Message *CreateMessage()
{
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(PAYLOAD_LENGTH));
    TEST_ASSERT_NOT_NULL(payload);
    for (uint32_t index = 0; index < PAYLOAD_LENGTH; index++)
    {
        payload[index] = static_cast<uint8_t>(index + 1);
    }
    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, 7, StatusCodes::CompletedOk, 99, payload, PAYLOAD_LENGTH);
    TEST_ASSERT_NOT_NULL(message);
    message->PacketOffset = 0;
    message->PacketLength = PAYLOAD_LENGTH;
    return (message);
}

/**
 * @brief Allocate a block of exactly a size, filled with the sentinel, so that a write past it is a write past the block.
 *
 * @param length Size of the block.
 *
 * @return The block, which the caller releases.
 */
static uint8_t *CreateBuffer(const uint32_t length)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(length));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, SENTINEL, length);
    return (buffer);
}

/**
 * @brief Check that every byte of a buffer is still the sentinel.
 *
 * @param buffer Buffer to check.
 * @param length Number of bytes in it.
 *
 * @return true if nothing was written.
 */
static bool Untouched(const uint8_t *buffer, const uint32_t length)
{
    for (uint32_t index = 0; index < length; index++)
    {
        if (buffer[index] != SENTINEL)
        {
            return (false);
        }
    }
    return (true);
}

TEST_CASE("EncodeMessage fills a buffer of exactly the size the packet needs, and extracting it gives the message back", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t needed = Encoders::CalculateSpiBufferSize(Encoders::EncodedPacketSize(message, false));
    uint8_t *buffer = CreateBuffer(needed);

    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, buffer, needed, false));

    Message *decoded = Encoders::ExtractMessage(buffer, needed, false);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_UINT32(message->Function, decoded->Function);
    TEST_ASSERT_EQUAL_UINT32(message->MessageID, decoded->MessageID);
    TEST_ASSERT_EQUAL_UINT32(PAYLOAD_LENGTH, decoded->PayloadLength);
    TEST_ASSERT_EQUAL_UINT16(PAYLOAD_LENGTH, decoded->PacketLength);
    TEST_ASSERT_EQUAL_MEMORY(message->Payload, decoded->Payload, PAYLOAD_LENGTH);

    Message::DeleteMessage(decoded);
    vPortFree(buffer);
    Message::DeleteMessage(message);
}

TEST_CASE("EncodeMessage refuses a buffer too small for the packet and writes nothing", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t needed = Encoders::CalculateSpiBufferSize(Encoders::EncodedPacketSize(message, false));

    //
    //  One byte short, and short by the whole payload.
    //
    const uint32_t lengths[] = {needed - 1, Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE), 0};
    for (const uint32_t length: lengths)
    {
        uint8_t *buffer = CreateBuffer(length == 0 ? 1 : length);
        TEST_ASSERT_FALSE_MESSAGE(Encoders::EncodeMessage(message, buffer, length, false), "A message was encoded into a buffer too small for it.");
        TEST_ASSERT_TRUE_MESSAGE(Untouched(buffer, length == 0 ? 1 : length), "A refused message was partly written.");
        vPortFree(buffer);
    }

    Message::DeleteMessage(message);
}

TEST_CASE("EncodeMessage needs only the header's room for a header only packet", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t needed = Encoders::CalculateSpiBufferSize(Encoders::EncodedPacketSize(message, true));
    uint8_t *buffer = CreateBuffer(needed);

    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, buffer, needed, true));
    Message *decoded = Encoders::ExtractMessage(buffer, needed, true);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_NULL(decoded->Payload);

    Message::DeleteMessage(decoded);
    vPortFree(buffer);
    Message::DeleteMessage(message);
}

TEST_CASE("EncodeMessage refuses a message whose packet is not inside its payload", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t length = Message::MAXIMUM_SPI_FRAME_SIZE;
    uint8_t *buffer = CreateBuffer(length);

    //
    //  A packet that claims more than the payload holds would be copied from past the end of it.
    //
    message->PacketLength = PAYLOAD_LENGTH + 1;
    TEST_ASSERT_FALSE_MESSAGE(Encoders::EncodeMessage(message, buffer, length, false), "A packet longer than the payload was encoded.");
    message->PacketLength = 10;
    message->PacketOffset = PAYLOAD_LENGTH - 5;
    TEST_ASSERT_FALSE_MESSAGE(Encoders::EncodeMessage(message, buffer, length, false), "A packet running past the end of the payload was encoded.");
    TEST_ASSERT_TRUE(Untouched(buffer, length));

    //
    //  A packet with no payload to copy it from.
    //
    message->PacketOffset = 0;
    message->PacketLength = PAYLOAD_LENGTH;
    uint8_t *payload = message->Payload;
    message->Payload = nullptr;
    TEST_ASSERT_FALSE_MESSAGE(Encoders::EncodeMessage(message, buffer, length, false), "A packet with no payload was encoded.");
    TEST_ASSERT_TRUE(Untouched(buffer, length));
    message->Payload = payload;

    TEST_ASSERT_FALSE(Encoders::EncodeMessage(nullptr, buffer, length, false));
    TEST_ASSERT_FALSE(Encoders::EncodeMessage(message, nullptr, length, false));

    vPortFree(buffer);
    Message::DeleteMessage(message);
}

TEST_CASE("ExtractMessage refuses a buffer too short to hold a header", CLASS_OF_TESTS)
{
    //
    //  The CRC covers all but the last four bytes, so a length below four used to wrap to a huge one.
    //
    const uint32_t minimum = Message::HEADER_SIZE + Message::SPI_MESSAGE_OVERHEAD;
    for (uint32_t length = 0; length < minimum; length++)
    {
        uint8_t *buffer = CreateBuffer(length == 0 ? 1 : length);
        TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(buffer, length, true), "A buffer too short for a header was extracted.");
        vPortFree(buffer);
    }
    TEST_ASSERT_NULL(Encoders::ExtractMessage(nullptr, minimum, true));

    uint8_t *tooLong = CreateBuffer(Message::MAXIMUM_SPI_FRAME_SIZE + 1);
    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(tooLong, Message::MAXIMUM_SPI_FRAME_SIZE + 1, true), "A buffer longer than a frame was extracted.");
    vPortFree(tooLong);
}

TEST_CASE("ExtractMessage leaves the buffer as it found it", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t needed = Encoders::CalculateSpiBufferSize(Encoders::EncodedPacketSize(message, false));
    uint8_t *buffer = CreateBuffer(needed);
    uint8_t *copy = CreateBuffer(needed);
    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, buffer, needed, false));
    memcpy(copy, buffer, needed);

    Message *decoded = Encoders::ExtractMessage(buffer, needed, false);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(copy, buffer, needed, "ExtractMessage changed the buffer it was given.");

    //
    //  So extracting it again works, which it did not when the CRC field had been zeroed.
    //
    Message *again = Encoders::ExtractMessage(buffer, needed, false);
    TEST_ASSERT_NOT_NULL(again);

    Message::DeleteMessage(again);
    Message::DeleteMessage(decoded);
    vPortFree(copy);
    vPortFree(buffer);
    Message::DeleteMessage(message);
}

TEST_CASE("ExtractMessage refuses a packet that declares more payload than the buffer holds, whatever the CRC says", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    const uint32_t needed = Encoders::CalculateSpiBufferSize(Encoders::EncodedPacketSize(message, false));
    uint8_t *buffer = CreateBuffer(needed);
    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, buffer, needed, false));

    //
    //  Raise the packet length past what the buffer holds and recompute the CRC over the new header, as a
    //  peer that wanted to read past the end would.  The packet length is at offset 7 (protocol, CRC, offset).
    //
    const uint16_t declared = static_cast<uint16_t>(needed);
    Encoders::EncodeUInt16(declared, buffer + 7);
    Encoders::EncodeUInt32(0, buffer + Message::CRC_OFFSET);
    Encoders::EncodeUInt32(Encoders::CRC32(buffer, needed - Message::SPI_MESSAGE_OVERHEAD), buffer + Message::CRC_OFFSET);

    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(buffer, needed, false), "A packet declaring more payload than the buffer holds was extracted.");

    //
    //  Its header is still good when the payload is not wanted.
    //
    Message *header = Encoders::ExtractMessage(buffer, needed, true);
    TEST_ASSERT_NOT_NULL(header);
    Message::DeleteMessage(header);

    vPortFree(buffer);
    Message::DeleteMessage(message);
}
