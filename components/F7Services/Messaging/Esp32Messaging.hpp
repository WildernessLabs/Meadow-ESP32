/**
 * @file Esp32Messaging.hpp
 * @author Mark Stevens
 * @brief Generic messaging constants, structures and definitions used in the messaging system.
 * @date 2019-03-15
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include <inttypes.h>

/*
 *  Messages between the ESP32 and the STM32 will be encoded and
 *  extracted through the message structure blow.
 */
struct Message
{
    /**
     *  Type of message.
     */
    uint8_t MessageType;

    /**
     *  Interface that this message is destined for.
     */
    uint8_t Interface;

    /**
     *  Function (on the interface) to be executed.
     */
    uint32_t Function;

    /**
     *  Status code (for returning messages) from the function.
     */
    uint32_t StatusCode;

    /**
     *  Unique ID of this message.
     */
    uint32_t MessageID;

    /**
     *  @brief Offset of the packet in the full message.
     */
    uint16_t PacketOffset;

    /**
     *  @brief Number of bytes in the packet.
     */
    uint16_t PacketLength;

    /**
     *  Pointer to the payload data to be processed (or returned from) the function.
     */
    uint8_t *Payload;

    /**
     *  Number of bytes in the payload.
     */
    uint32_t PayloadLength;

#if defined(PERFORMANCE_LOGGING)
    int64_t PerformanceLoggingStart;
#endif

    /**
     *  @brief Size of an encoded message header (in bytes).
     */
    static const uint32_t HEADER_SIZE = 27;

    /**
     *  @brief Offset of the CRC in an encoded message header.
     */
    static const uint32_t CRC_OFFSET = 1;

    /*
     *  @brief Current protocol number.
     */
    static const uint8_t PROTOCOL_NUMBER = 1;

    /**
     *  @brief Maximum number of bytes in an SPI frame.
     *
     *  This is determined by a bug in the ESP where the maximum number of bytes in a SPI
     *  data transmission is 4092 bytes.
     */
    static const uint16_t MAXIMUM_SPI_FRAME_SIZE = 4092;

    /**
     * @brief SPI overhead in bytes.
     *
     *  The buffer should always be 4 bytes longer than needed.  During development it
     *  was found that the last four bytes of any transmission were being discarded.
     *  Empirical tests proved this for 24, 32 and 40 byte packets.
     *
     *  The work around is to increase the packet size by 4 and have dummy data in the
     *  last four bytes and discard the bytes.
     *
     * See support post: https://esp32.com/viewtopic.php?f=13&t=10117
     */
    static const uint16_t SPI_MESSAGE_OVERHEAD = 4;

    /**
     *  @brief Maximum number of payload bytes in a payload frame (packet).
     */
    static const uint16_t MAXIMUM_PACKET_SIZE = (MAXIMUM_SPI_FRAME_SIZE - HEADER_SIZE - SPI_MESSAGE_OVERHEAD);

    /**
     *  @brief Maximum number of bytes in a fully reassembled message payload.
     *
     *  A payload above MAXIMUM_PACKET_SIZE is split across several packets by
     *  SpiTransportProvider and reassembled by the receiver, so this is a protocol-level
     *  bound rather than a per-packet one.  SpiTransportProvider::ProcessInboundPacket
     *  rejects a PayloadLength above this when reassembly starts, so decoders elsewhere
     *  can still treat it as a hard upper bound to scan against without re-checking it
     *  themselves.
     */
    static const uint32_t MAXIMUM_PAYLOAD_LENGTH = 8192;

    /**
     *  @brief Message ID used to indicate an invalid (or unknown) message ID.
     */
    static const uint32_t INVALID_MESSAGE_ID = 0xffffffff;

    //
    //  A Message is plain data.  Every one is allocated with pvPortMalloc, by CreateOnHeap,
    //  CreateCopyOnHeap or a decoder, and released with DeleteMessage, so no constructor
    //  would ever run and none is declared.  CreateOnHeap zeroes the whole structure.
    //

    /**
     *  @brief Deleted copy constructor: a copy would share Payload and release it twice.
     *         Use CreateCopyOnHeap to copy a message.
     */
    Message(const Message &) = delete;

    /**
     *  @brief Deleted copy assignment operator, for the same reason as the copy constructor.
     */
    Message &operator=(const Message &) = delete;

    /**
     *  @brief Make a copy of the message.
     */
    Message *CreateCopyOnHeap(const bool copyPayload);

    /**
     *  @brief Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(const uint8_t messageType, const uint8_t interface, const uint32_t function, const uint32_t statusCode, const uint32_t messageId, uint8_t *payload, const uint32_t payloadLength);

    /**
     *  @brief Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(const uint8_t messageType, const uint8_t interface, const uint32_t function, const uint32_t statusCode);

    /**
     *  @brief Delete the payload and free up heap storage.
     */
    void DeletePayload();

    /**
     *  @brief Delete the message and payload releasing heap storage, and set the caller's pointer to nullptr.
     */
    static void DeleteMessage(Message *&message);
};
