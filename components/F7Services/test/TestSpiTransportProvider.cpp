/**
 * @file TestSpiTransportProvider.cpp
 * @brief Test methods for SpiTransportProvider's inbound packet reassembly.
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string.h>

#include "unity.h"

#include "SpiTransportProvider.hpp"
#include "Encoders.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  These drive SpiTransportProvider::ProcessInboundPacket directly with synthetic
 *  Message objects standing in for packets from the STM32, so the reassembly-buffer
 *  validation added for issue #215 can be exercised without a real SPI peer.
 *  ProcessInboundPacket only manipulates heap buffers and the private state below; it
 *  never queues an actual SPI transaction with the hardware, so this is safe to run
 *  under QEMU.
 *
 *  SpiTransportProvider is guarded the same way as WiFiRequestHandler and the other
 *  singleton handlers: a second instance resets the board by design.  Production
 *  builds it once and never destroys it, so these tests do the same rather than
 *  constructing and destroying one per test - the shared instance is built on first
 *  use and each test resets the one piece of private state it touches (_incomingMessage)
 *  instead of tearing the object down.
 */
#define CLASS_OF_TESTS "[SpiTransportProvider][Emulated]"

/**
 * @brief Friend of SpiTransportProvider granting these tests access to
 *  ProcessInboundPacket and the private state it reads and writes.
 */
class TestSpiTransportProvider
{
public:
    /**
     * @brief Call the private ProcessInboundPacket method.
     */
    static spi_slave_transaction_t *CallProcessInboundPacket(SpiTransportProvider *provider, Message *packet)
    {
        return (provider->ProcessInboundPacket(packet));
    }

    /**
     * @brief Read the private _incomingMessage field.
     */
    static Message *GetIncomingMessage(SpiTransportProvider *provider)
    {
        return (provider->_incomingMessage);
    }

    /**
     * @brief Overwrite the private _incomingMessage field.
     */
    static void SetIncomingMessage(SpiTransportProvider *provider, Message *message)
    {
        provider->_incomingMessage = message;
    }

    /**
     * @brief Read the private, static _headerFrameSize field.
     */
    static uint32_t GetHeaderFrameSize()
    {
        return (SpiTransportProvider::_headerFrameSize);
    }

    /**
     * @brief Call the private DeleteTransaction method.
     */
    static void CallDeleteTransaction(SpiTransportProvider *provider, spi_slave_transaction_t *&transaction)
    {
        provider->DeleteTransaction(transaction);
    }
};

/**
 * @brief The one SpiTransportProvider instance these tests share.
 */
static SpiTransportProvider *_provider = nullptr;

/**
 * @brief Build the shared instance if it does not already exist.
 *
 * @return Pointer to the shared instance.
 */
static SpiTransportProvider *EnsureProvider()
{
    if (_provider == nullptr)
    {
        _provider = new SpiTransportProvider();
    }

    return (_provider);
}

/**
 * @brief Release whatever the previous test left in _incomingMessage, so each test
 *  starts from a known, empty state.
 */
static void ResetIncomingMessage()
{
    Message *leftover = TestSpiTransportProvider::GetIncomingMessage(_provider);

    if (leftover != nullptr)
    {
        Message::DeleteMessage(leftover);
        TestSpiTransportProvider::SetIncomingMessage(_provider, nullptr);
    }
}

/**
 * @brief Build a synthetic packet as if it had just arrived from the STM32.
 *
 * @param packetLength Number of bytes actually carried in this packet.
 * @param payloadLength Full payload length the packet claims the eventual message
 *  will have.
 *
 * @return The packet, on the heap, matching what ExtractMessage would hand
 *  ProcessInboundPacket in production.
 */
static Message *BuildPacket(const uint16_t packetLength, const uint32_t payloadLength)
{
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(packetLength));
    TEST_ASSERT_NOT_NULL(payload);
    for (uint16_t index = 0; index < packetLength; index++)
    {
        payload[index] = static_cast<uint8_t>(index);
    }

    Message *packet = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, 0, StatusCodes::CompletedOk, 1, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(packet);
    packet->PacketLength = packetLength;

    return (packet);
}

/**
 * @brief Decode the MessageType and StatusCode from the ack/nak transaction
 *  ProcessInboundPacket returns, then release the transaction.
 *
 * @param transaction Transaction returned by ProcessInboundPacket.  Released by this
 *  call.
 * @param messageType Filled in with the decoded MessageType.
 * @param statusCode Filled in with the decoded StatusCode.
 */
static void DecodeAndFreeAcknowledgement(spi_slave_transaction_t *transaction, uint8_t *messageType, uint32_t *statusCode)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(transaction, "ProcessInboundPacket did not return an acknowledgement transaction.");

    uint8_t *txBuffer = static_cast<uint8_t *>(const_cast<void *>(transaction->tx_buffer));
    Message *decoded = Encoders::ExtractMessage(txBuffer, TestSpiTransportProvider::GetHeaderFrameSize(), true);
    TEST_ASSERT_NOT_NULL_MESSAGE(decoded, "The acknowledgement could not be decoded; its CRC did not match.");

    *messageType = decoded->MessageType;
    *statusCode = decoded->StatusCode;

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&decoded));
    TestSpiTransportProvider::CallDeleteTransaction(_provider, transaction);
}

/**
 * @brief Check that a valid first fragment of a multi-packet message is accepted,
 *  copied into a freshly allocated reassembly buffer, and kept as _incomingMessage
 *  pending the rest of the message.
 *
 *  Regression check for issue #215: this is the success path the fix's new checks sit
 *  next to, so a mistake in those checks would show up here as a broken success case.
 */
TEST_CASE("ProcessInboundPacket accepts a valid first fragment of a multi-packet message", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    Message *packet = BuildPacket(4, 100);

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallProcessInboundPacket(provider, packet);

    uint8_t messageType = 0;
    uint32_t statusCode = 0;
    DecodeAndFreeAcknowledgement(transaction, &messageType, &statusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Ack, messageType);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, statusCode);

    Message *incoming = TestSpiTransportProvider::GetIncomingMessage(provider);
    TEST_ASSERT_NOT_NULL_MESSAGE(incoming, "The first fragment should have been kept, awaiting the rest of the message.");
    TEST_ASSERT_EQUAL_UINT32(100, incoming->PayloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(packet->Payload, incoming->Payload, 4);

    ResetIncomingMessage();
}

/**
 * @brief Check that a packet claiming more bytes than the message's declared payload
 *  length is refused rather than overrunning the reassembly buffer.
 *
 *  Regression check for issue #215.  Before the fix, PacketLength (100) exceeding
 *  PayloadLength (10) meant pvPortMalloc(10) was followed by memcpy of 100 bytes -
 *  a heap buffer overflow.
 */
TEST_CASE("ProcessInboundPacket rejects a fragment whose PacketLength exceeds PayloadLength", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    Message *packet = BuildPacket(100, 10);

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallProcessInboundPacket(provider, packet);
    //
    //  The packet is deleted internally by ProcessInboundPacket on this path; it must
    //  not be touched again here.
    //

    uint8_t messageType = 0;
    uint32_t statusCode = 0;
    DecodeAndFreeAcknowledgement(transaction, &messageType, &statusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Nak, messageType);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, statusCode);

    TEST_ASSERT_NULL_MESSAGE(TestSpiTransportProvider::GetIncomingMessage(provider), "A rejected fragment must not become the pending incoming message.");
}

/**
 * @brief Check that a payload length past the protocol maximum is refused rather than
 *  handed to pvPortMalloc.
 *
 *  Regression check for issue #215's other new guard: PayloadLength above
 *  Message::MAXIMUM_PAYLOAD_LENGTH is never legitimate, however it relates to
 *  PacketLength.
 */
TEST_CASE("ProcessInboundPacket rejects a PayloadLength above the protocol maximum", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    Message *packet = BuildPacket(10, Message::MAXIMUM_PAYLOAD_LENGTH + 1);

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallProcessInboundPacket(provider, packet);

    uint8_t messageType = 0;
    uint32_t statusCode = 0;
    DecodeAndFreeAcknowledgement(transaction, &messageType, &statusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Nak, messageType);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, statusCode);

    TEST_ASSERT_NULL_MESSAGE(TestSpiTransportProvider::GetIncomingMessage(provider), "A rejected fragment must not become the pending incoming message.");
}
