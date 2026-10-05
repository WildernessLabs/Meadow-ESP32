/*
 *  TestEncodersMessage.cpp
 *
 *  Tests that EncodeMessage writes only inside the buffer it is given, that ExtractMessage reads only
 *  inside the buffer it is given and leaves it as it was, and that both refuse what does not fit
 *  (issue #267).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <stdio.h>
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

/**
 * @brief Offset of the packet length in an encoded packet: the protocol number, the CRC and the packet offset.
 */
#define PACKET_LENGTH_OFFSET 7

/**
 * @brief Offset of the payload length in an encoded packet: the last field of the header.
 */
#define PAYLOAD_LENGTH_OFFSET (Message::HEADER_SIZE - 4)

/**
 * @brief Calculate the CRC of an encoded packet again, as a sender would after changing its header.
 *
 * @param buffer The encoded packet.
 * @param length Number of bytes in it.
 */
static void Reseal(uint8_t *buffer, const uint32_t length)
{
    Encoders::EncodeUInt32(0, buffer + Message::CRC_OFFSET);
    Encoders::EncodeUInt32(Encoders::CRC32(buffer, length - Message::SPI_MESSAGE_OVERHEAD), buffer + Message::CRC_OFFSET);
}

/**
 * @brief Encode a message into a whole SPI frame, as the STM32 sends it.
 *
 * @param message Message to encode.
 *
 * @return The frame, which the caller releases.
 */
static uint8_t *EncodeIntoFrame(const Message *message)
{
    uint8_t *frame = CreateBuffer(Message::MAXIMUM_SPI_FRAME_SIZE);
    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, frame, Message::MAXIMUM_SPI_FRAME_SIZE, false));
    return (frame);
}

TEST_CASE("ExtractMessage refuses a packet length of 0xFFFF in a full frame", CLASS_OF_TESTS)
{
    //
    //  A frame of 4092 bytes declaring a packet of 0xFFFF used to be copied from about 61KB past
    //  the start of the buffer, taking the heap that follows it (issue #216).
    //
    Message *message = CreateMessage();
    uint8_t *frame = EncodeIntoFrame(message);
    Encoders::EncodeUInt16(0xffff, frame + PACKET_LENGTH_OFFSET);
    Reseal(frame, Message::MAXIMUM_SPI_FRAME_SIZE);

    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, false), "A packet of 0xFFFF bytes was extracted from a 4092 byte frame.");
    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, true), "A header declaring a packet of 0xFFFF bytes was accepted.");

    vPortFree(frame);
    Message::DeleteMessage(message);
}

TEST_CASE("ExtractMessage accepts the longest packet the protocol allows and refuses one byte more", CLASS_OF_TESTS)
{
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(Message::MAXIMUM_PACKET_SIZE + 1));
    TEST_ASSERT_NOT_NULL(payload);
    for (uint32_t index = 0; index < Message::MAXIMUM_PACKET_SIZE + 1; index++)
    {
        payload[index] = static_cast<uint8_t>(index);
    }
    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, 7, StatusCodes::CompletedOk, 99, payload, Message::MAXIMUM_PACKET_SIZE + 1);
    TEST_ASSERT_NOT_NULL(message);
    message->PacketOffset = 0;
    message->PacketLength = Message::MAXIMUM_PACKET_SIZE;

    uint8_t *frame = EncodeIntoFrame(message);
    Message *decoded = Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, false);
    TEST_ASSERT_NOT_NULL_MESSAGE(decoded, "The longest packet the protocol allows was refused.");
    TEST_ASSERT_EQUAL_UINT16(Message::MAXIMUM_PACKET_SIZE, decoded->PacketLength);
    TEST_ASSERT_EQUAL_MEMORY(payload, decoded->Payload, Message::MAXIMUM_PACKET_SIZE);
    Message::DeleteMessage(decoded);

    Encoders::EncodeUInt16(Message::MAXIMUM_PACKET_SIZE + 1, frame + PACKET_LENGTH_OFFSET);
    Reseal(frame, Message::MAXIMUM_SPI_FRAME_SIZE);
    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, false), "A packet one byte longer than the protocol allows was extracted.");

    vPortFree(frame);
    Message::DeleteMessage(message);
}

TEST_CASE("ExtractMessage refuses a message that announces a payload and carries none of it", CLASS_OF_TESTS)
{
    //
    //  A packet length of zero with a payload to follow made a zero byte allocation, then a copy of that length.
    //
    Message *message = CreateMessage();
    uint8_t *frame = EncodeIntoFrame(message);
    Encoders::EncodeUInt16(0, frame + PACKET_LENGTH_OFFSET);
    Reseal(frame, Message::MAXIMUM_SPI_FRAME_SIZE);

    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, false), "A message announcing a payload and carrying none was extracted.");

    //
    //  A header on its own, which carries no payload, is still what it was.
    //
    Message *header = Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, true);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_EQUAL_UINT32(PAYLOAD_LENGTH, header->PayloadLength);
    Message::DeleteMessage(header);

    //
    //  And a message that announces no payload may carry no packet.
    //
    Encoders::EncodeUInt32(0, frame + PAYLOAD_LENGTH_OFFSET);
    Reseal(frame, Message::MAXIMUM_SPI_FRAME_SIZE);
    Message *empty = Encoders::ExtractMessage(frame, Message::MAXIMUM_SPI_FRAME_SIZE, false);
    TEST_ASSERT_NOT_NULL_MESSAGE(empty, "A message with no payload was refused.");
    TEST_ASSERT_NULL(empty->Payload);
    Message::DeleteMessage(empty);

    vPortFree(frame);
    Message::DeleteMessage(message);
}

/**
 * @brief Set the CRC in an encoded packet to a value that cannot be right.
 *
 * @param buffer The encoded packet.
 */
static void SpoilCrc(uint8_t *buffer)
{
    Encoders::EncodeUInt32(Encoders::ExtractUInt32(buffer + Message::CRC_OFFSET) ^ 0x00000001, buffer + Message::CRC_OFFSET);
}

TEST_CASE("Whether a packet is well formed does not depend on its CRC", CLASS_OF_TESTS)
{
    //
    //  The CRC shows a frame arrived as it was sent, not that it is well formed, so the structure is
    //  checked on its own, and a frame has to pass that and the CRC (issue #277).
    //
    Message *message = CreateMessage();
    uint8_t *frame = EncodeIntoFrame(message);
    const uint32_t length = Message::MAXIMUM_SPI_FRAME_SIZE;
    //
    //  The encoder's CRC covers the packet and its padding, not the whole frame, so seal the frame as received.
    //
    Reseal(frame, length);

    TEST_ASSERT_TRUE(Encoders::PacketIsWellFormed(frame, length, false));
    Message *good = Encoders::ExtractMessage(frame, length, false);
    TEST_ASSERT_NOT_NULL(good);
    Message::DeleteMessage(good);

    //
    //  A wrong CRC on a well formed packet: still well formed, and refused by the CRC.
    //
    SpoilCrc(frame);
    TEST_ASSERT_TRUE_MESSAGE(Encoders::PacketIsWellFormed(frame, length, false), "A packet's structure was judged by its CRC.");
    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, length, false), "A packet with a wrong CRC was extracted.");
    TEST_ASSERT_NULL(Encoders::ExtractMessage(frame, length, true));

    //
    //  A bad length with a right CRC, and with a wrong one, is not well formed either way.
    //
    Encoders::EncodeUInt16(0xffff, frame + PACKET_LENGTH_OFFSET);
    TEST_ASSERT_FALSE_MESSAGE(Encoders::PacketIsWellFormed(frame, length, false), "A packet with a bad length was judged well formed because its CRC was wrong.");
    TEST_ASSERT_FALSE(Encoders::PacketIsWellFormed(frame, length, true));
    Reseal(frame, length);
    TEST_ASSERT_FALSE_MESSAGE(Encoders::PacketIsWellFormed(frame, length, false), "A packet with a bad length was judged well formed because its CRC was right.");
    TEST_ASSERT_NULL(Encoders::ExtractMessage(frame, length, false));

    //
    //  A packet announcing a payload and carrying none, with a wrong CRC.
    //
    Encoders::EncodeUInt16(0, frame + PACKET_LENGTH_OFFSET);
    SpoilCrc(frame);
    TEST_ASSERT_FALSE(Encoders::PacketIsWellFormed(frame, length, false));
    TEST_ASSERT_TRUE_MESSAGE(Encoders::PacketIsWellFormed(frame, length, true), "A header on its own was judged by the payload it announces.");

    //
    //  Buffers that cannot hold a header, and none at all.
    //
    TEST_ASSERT_FALSE(Encoders::PacketIsWellFormed(nullptr, length, false));
    TEST_ASSERT_FALSE(Encoders::PacketIsWellFormed(frame, Message::HEADER_SIZE + Message::SPI_MESSAGE_OVERHEAD - 1, true));
    TEST_ASSERT_FALSE(Encoders::PacketIsWellFormed(frame, Message::MAXIMUM_SPI_FRAME_SIZE + 1, true));

    vPortFree(frame);
    Message::DeleteMessage(message);
}

TEST_CASE("A well formed packet whose contents were damaged is refused by the CRC", CLASS_OF_TESTS)
{
    Message *message = CreateMessage();
    uint8_t *frame = EncodeIntoFrame(message);
    const uint32_t length = Message::MAXIMUM_SPI_FRAME_SIZE;
    //
    //  The encoder's CRC covers the packet and its padding, not the whole frame, so seal the frame as received.
    //
    Reseal(frame, length);

    frame[Message::HEADER_SIZE + 3] ^= 0x40;
    TEST_ASSERT_TRUE(Encoders::PacketIsWellFormed(frame, length, false));
    TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractMessage(frame, length, false), "A frame damaged in transmission was extracted.");

    vPortFree(frame);
    Message::DeleteMessage(message);
}

/**
 * @brief The SPI buffer is a multiple of 4 bytes and never smaller than 8 plus the overhead (issue #273).
 *
 *  The minimum used to be overwritten by the alignment, so a request of 1 to 3 bytes was given 4.
 */
TEST_CASE("CalculateSpiBufferSize rounds up to a multiple of 4 and is never below the minimum", CLASS_OF_TESTS)
{
    static constexpr uint32_t MINIMUM = 8;
    //
    //  Each pair is a requested size and the size it is rounded up to before the overhead is added.
    //
    const uint32_t cases[][2] = {{0, MINIMUM}, {1, MINIMUM}, {2, MINIMUM}, {3, MINIMUM}, {4, MINIMUM}, {5, MINIMUM}, {7, MINIMUM}, {8, 8}, {9, 12}, {12, 12}, {13, 16}, {4092, 4092}, {4093, 4096}};
    for (const auto &entry: cases)
    {
        char message[48];
        snprintf(message, sizeof(message), "A request of %lu bytes.", static_cast<unsigned long>(entry[0]));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(entry[1] + Message::SPI_MESSAGE_OVERHEAD, Encoders::CalculateSpiBufferSize(entry[0]), message);
    }
}
