/**
 * @file TestSpiTransportProvider.cpp
 * @brief Test methods for SpiTransportProvider's inbound packet reassembly.
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string.h>

#include "esp_memory_utils.h"
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
 *
 *  They run under QEMU but are not tagged [Emulated].  Building the provider starts its
 *  synchronisation task, which prints +++RDY to the console for the rest of the boot and
 *  can make the test runner lose a later selector, so run-tests-ci.sh runs them last.
 *  See issue #499.
 */
#define CLASS_OF_TESTS "[SpiTransportProvider]"

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

    /**
     * @brief Call the private CreateAcknowledgementTransaction method.
     */
    static spi_slave_transaction_t *CallCreateAcknowledgementTransaction(SpiTransportProvider *provider, Message *message, const StatusCodes::StatusCodes code, const uint8_t messageType)
    {
        return (provider->CreateAcknowledgementTransaction(message, code, messageType));
    }

    /**
     * @brief Read the private, static _txBuffer field.
     */
    static uint8_t *GetTransmitBuffer()
    {
        return (SpiTransportProvider::_txBuffer);
    }

    /**
     * @brief Read the private, static _rxBuffer field.
     */
    static uint8_t *GetReceiveBuffer()
    {
        return (SpiTransportProvider::_rxBuffer);
    }

    /**
     * @brief Call the private CreateSpiTransaction method.
     */
    static spi_slave_transaction_t *CallCreateSpiTransaction(SpiTransportProvider *provider, Message *message)
    {
        return (provider->CreateSpiTransaction(message));
    }

    /**
     * @brief Call the private PacketDetailsSent method.
     */
    static spi_slave_transaction_t *CallPacketDetailsSent(SpiTransportProvider *provider, spi_slave_transaction_t *transaction)
    {
        return (provider->PacketDetailsSent(transaction));
    }

    /**
     * @brief Call the private PacketAcknowledgementReceived method.
     */
    static spi_slave_transaction_t *CallPacketAcknowledgementReceived(SpiTransportProvider *provider, spi_slave_transaction_t *transaction)
    {
        return (provider->PacketAcknowledgementReceived(transaction));
    }

    /**
     * @brief Read the private _outboundMessage field.
     */
    static Message *GetOutboundMessage(SpiTransportProvider *provider)
    {
        return (provider->_outboundMessage);
    }

    /**
     * @brief Overwrite the private _outboundMessage field.
     */
    static void SetOutboundMessage(SpiTransportProvider *provider, Message *message)
    {
        provider->_outboundMessage = message;
    }

    /**
     * @brief Call the private post setup callback, as the SPI interrupt does.
     */
    static void CallPostSetupCallback()
    {
        SpiTransportProvider::SpiPostSetupCallback(nullptr);
    }

    /**
     * @brief Address of the post setup callback.
     */
    static const void *PostSetupCallbackAddress()
    {
        return (reinterpret_cast<const void *>(&SpiTransportProvider::SpiPostSetupCallback));
    }

    /**
     * @brief Overwrite the private, static instance pointer the callback uses.
     *
     * @return The pointer that was there.
     */
    static SpiTransportProvider *ExchangeInstance(SpiTransportProvider *instance)
    {
        SpiTransportProvider * const previous = SpiTransportProvider::_spiInstance;
        SpiTransportProvider::_spiInstance = instance;
        return (previous);
    }

    /**
     * @brief Overwrite the task handle the callback notifies.
     *
     * @return The handle that was there.
     */
    static TaskHandle_t ExchangeTaskHandle(TaskHandle_t handle)
    {
        TaskHandle_t const previous = SpiTransportProvider::_transportProviderTaskHandle;
        SpiTransportProvider::_transportProviderTaskHandle = handle;
        return (previous);
    }

    /**
     * @brief Overwrite the semaphore the callback gives.
     *
     * @return The semaphore that was there.
     */
    static SemaphoreHandle_t ExchangeSemaphore(SpiTransportProvider *provider, SemaphoreHandle_t semaphore)
    {
        SemaphoreHandle_t const previous = provider->_synchronisationSemaphore;
        provider->_synchronisationSemaphore = semaphore;
        return (previous);
    }

    /**
     * @brief Address of the first instruction of a member function.
     *
     *  A member function pointer is not convertible to a pointer, but on this ABI its first word
     *  is the address of a function that is not virtual, which these are not.
     */
    template<typename MemberFunction> static const void *MemberFunctionAddress(const MemberFunction function)
    {
        const void *address = nullptr;
        memcpy(&address, &function, sizeof(address));
        return (address);
    }

    /**
     * @brief Addresses of the control signal accessors the post setup callback calls.
     */
    static void ControlSignalAccessorAddresses(const void *addresses[3])
    {
        addresses[0] = MemberFunctionAddress(&TransportProviderBase::GetControlSignalMessage);
        addresses[1] = MemberFunctionAddress(&TransportProviderBase::SetControlSignalMessage);
        addresses[2] = MemberFunctionAddress(&TransportProviderBase::SetControlSignalDelay);
    }

    /**
     * @brief Put the private, static state machine back in its idle state.
     */
    static void ResetState()
    {
        SpiTransportProvider::_currentState = SpiTransportProvider::WaitingForPacket;
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

    Utils::DeleteAndNullifyPointer(decoded);
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

/**
 * @brief Check that an acknowledgement transaction holds nothing from the message it was made from.
 *
 *  CreateAcknowledgementTransaction deletes its copy of the message as soon as the
 *  transaction exists (issue #235).  The test build poisons freed heap, so any field the
 *  transaction still read through that copy, or through the original once it is also
 *  deleted here, would decode as poison rather than as the acknowledgement.
 */
TEST_CASE("An acknowledgement transaction holds nothing from the message it was made from", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    const uint32_t MESSAGE_ID = 0x12345678;
    const uint32_t FUNCTION = 0x0a0b0c0d;
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(32));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 0xa5, 32);
    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, FUNCTION, StatusCodes::CompletedOk, MESSAGE_ID, payload, 32);
    TEST_ASSERT_NOT_NULL(message);

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateAcknowledgementTransaction(provider, message, StatusCodes::Failure, MessageTypes::Nak);
    TEST_ASSERT_NOT_NULL(transaction);
    Message::DeleteMessage(message);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(TestSpiTransportProvider::GetTransmitBuffer(), transaction->tx_buffer, "The transaction does not transmit from the provider's own buffer.");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(TestSpiTransportProvider::GetReceiveBuffer(), transaction->rx_buffer, "The transaction does not receive into the provider's own buffer.");

    uint8_t *txBuffer = static_cast<uint8_t *>(const_cast<void *>(transaction->tx_buffer));
    Message *decoded = Encoders::ExtractMessage(txBuffer, TestSpiTransportProvider::GetHeaderFrameSize(), true);
    TEST_ASSERT_NOT_NULL_MESSAGE(decoded, "The acknowledgement could not be decoded; its CRC did not match.");
    TEST_ASSERT_EQUAL_UINT8(MessageTypes::Nak, decoded->MessageType);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::Failure, decoded->StatusCode);
    TEST_ASSERT_EQUAL_UINT8(Esp32Interfaces::WiFi, decoded->Interface);
    TEST_ASSERT_EQUAL_UINT32(FUNCTION, decoded->Function);
    TEST_ASSERT_EQUAL_UINT32(MESSAGE_ID, decoded->MessageID);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, decoded->PayloadLength, "The acknowledgement declares a payload.");

    Utils::DeleteAndNullifyPointer(decoded);
    TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
}

/**
 * @brief Check that a transaction transmits the outbound message's current packet, encoded once.
 *
 * @param transaction Transaction to check.
 * @param message Outbound message, with its packet offset and length already advanced.
 * @param offset Packet offset expected.
 * @param length Packet length expected.
 */
static void CheckPacketTransaction(spi_slave_transaction_t *transaction, Message *message, const uint16_t offset, const uint16_t length)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(transaction, "No transaction was returned for the packet.");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(offset, message->PacketOffset, "The packet offset is wrong.");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(length, message->PacketLength, "The packet length is wrong.");

    uint8_t *expected = static_cast<uint8_t *>(pvPortMalloc(Message::MAXIMUM_SPI_FRAME_SIZE));
    TEST_ASSERT_NOT_NULL(expected);
    memset(expected, 0, Message::MAXIMUM_SPI_FRAME_SIZE);
    Encoders::EncodeMessage(message, expected, false);
    const bool same = (memcmp(expected, transaction->tx_buffer, Message::MAXIMUM_SPI_FRAME_SIZE) == 0);
    Utils::DeleteAndNullifyPointer(expected);
    TEST_ASSERT_TRUE_MESSAGE(same, "The transaction does not transmit the packet as EncodeMessage encodes it.");
}

/**
 * @brief Check that each packet of a multi-packet message is sent as one encoding of that packet.
 *
 *  PacketDetailsSent and PacketAcknowledgementReceived each encoded the packet into
 *  _txBuffer and then called CreateSpiTransaction, which cleared the buffer and encoded it
 *  again (issue #319).  Now only CreateSpiTransaction encodes it.  The frames sent must be
 *  unchanged, so this walks a three-packet message through both handlers.
 */
TEST_CASE("Each packet of an outbound message is transmitted as EncodeMessage encodes it", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    const uint32_t payloadLength = (2 * Message::MAXIMUM_PACKET_SIZE) + 10;
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    for (uint32_t index = 0; index < payloadLength; index++)
    {
        payload[index] = static_cast<uint8_t>(index * 7);
    }
    Message *message = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::WiFi, 3, StatusCodes::CompletedOk, 99, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(message);
    TestSpiTransportProvider::SetOutboundMessage(provider, message);

    //
    //  The header has been sent, so the first packet goes next.
    //
    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateSpiTransaction(provider, nullptr);
    transaction = TestSpiTransportProvider::CallPacketDetailsSent(provider, transaction);
    CheckPacketTransaction(transaction, message, 0, Message::MAXIMUM_PACKET_SIZE);

    //
    //  The STM32 acknowledges each packet, and the next one goes.
    //
    Message *acknowledgement = Message::CreateOnHeap(MessageTypes::Ack, Esp32Interfaces::Transport, 0, StatusCodes::CompletedOk, 99, nullptr, 0);
    TEST_ASSERT_NOT_NULL(acknowledgement);
    const uint16_t expectedOffsets[] = {Message::MAXIMUM_PACKET_SIZE, 2 * Message::MAXIMUM_PACKET_SIZE};
    const uint16_t expectedLengths[] = {Message::MAXIMUM_PACKET_SIZE, 10};
    for (uint32_t packet = 0; packet < 2; packet++)
    {
        Encoders::EncodeMessage(acknowledgement, static_cast<uint8_t *>(transaction->rx_buffer), true);
        transaction = TestSpiTransportProvider::CallPacketAcknowledgementReceived(provider, transaction);
        CheckPacketTransaction(transaction, message, expectedOffsets[packet], expectedLengths[packet]);
    }
    Message::DeleteMessage(acknowledgement);

    TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
    TestSpiTransportProvider::SetOutboundMessage(provider, nullptr);
    TestSpiTransportProvider::ResetState();
    Message::DeleteMessage(message);
}

/**
 * @brief Check how often the synchronisation task repeats the control signal it is sending.
 *
 *  SynchronisationTask sends the control signal whether its semaphore take succeeded or
 *  timed out (issue #323).  A timeout is the periodic resend that lets an STM32 that missed
 *  the first "+++RDY" see the next, so while the signal is Ready it is resent every two
 *  seconds.  SpiPostSetupCallback changes it to SpiInterfaceReady, and the delay to
 *  portMAX_DELAY, when the SPI interface is first set up, after which it is sent only when
 *  the semaphore is given.  Which of the two the shared provider is in depends on whether
 *  the board has had any SPI traffic, so either pair is accepted.  Changing the delay of
 *  either would change what the STM32 sees at start up.
 */
TEST_CASE("The control signal is resent every two seconds until the SPI interface is ready", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    const TransportProviderBase::ControlMessageType signal = provider->GetControlSignalMessage();
    const TickType_t delay = provider->GetControlSignalDelay();

    if (signal == TransportProviderBase::ControlMessageType::Ready)
    {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(pdMS_TO_TICKS(2000), delay, "The Ready signal is not resent every two seconds.");
    }
    else
    {
        TEST_ASSERT_EQUAL_MESSAGE(TransportProviderBase::ControlMessageType::SpiInterfaceReady, signal, "The control signal is neither Ready nor SpiInterfaceReady.");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(portMAX_DELAY, delay, "The SpiInterfaceReady signal is not sent only when signalled.");
    }
}

/**
 * @brief Check that the post setup callback and the functions it calls are in IRAM.
 *
 *  The SPI interrupt runs from IRAM so that it can run while flash is being written, and
 *  a call into flash from it then faults (issue #269).
 */
TEST_CASE("The SPI post setup callback and the functions it calls are in IRAM", CLASS_OF_TESTS)
{
    TEST_ASSERT_TRUE_MESSAGE(esp_ptr_in_iram(TestSpiTransportProvider::PostSetupCallbackAddress()), "SpiPostSetupCallback is not in IRAM.");

    const void *addresses[3] = {};
    TestSpiTransportProvider::ControlSignalAccessorAddresses(addresses);
    TEST_ASSERT_TRUE_MESSAGE(esp_ptr_in_iram(addresses[0]), "GetControlSignalMessage is not in IRAM.");
    TEST_ASSERT_TRUE_MESSAGE(esp_ptr_in_iram(addresses[1]), "SetControlSignalMessage is not in IRAM.");
    TEST_ASSERT_TRUE_MESSAGE(esp_ptr_in_iram(addresses[2]), "SetControlSignalDelay is not in IRAM.");
}

/**
 * @brief Check that the post setup callback does nothing while what it needs does not exist yet.
 *
 *  Notifying a task that does not exist asserts, which in the interrupt resets the board.  Each
 *  thing the callback needs is taken away in turn and put back.  The control signal must be as it
 *  was, as the callback is meant to do nothing at all.
 */
TEST_CASE("The SPI post setup callback does nothing before the task and semaphore exist", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    TEST_ASSERT_NOT_NULL(provider);

    const TransportProviderBase::ControlMessageType messageBefore = provider->GetControlSignalMessage();
    const TickType_t delayBefore = provider->GetControlSignalDelay();

    TaskHandle_t const taskHandle = TestSpiTransportProvider::ExchangeTaskHandle(nullptr);
    TestSpiTransportProvider::CallPostSetupCallback();
    TestSpiTransportProvider::ExchangeTaskHandle(taskHandle);
    TEST_ASSERT_EQUAL_MESSAGE(messageBefore, provider->GetControlSignalMessage(), "The callback changed the control signal without a task.");
    TEST_ASSERT_EQUAL(delayBefore, provider->GetControlSignalDelay());

    SemaphoreHandle_t const semaphore = TestSpiTransportProvider::ExchangeSemaphore(provider, nullptr);
    TestSpiTransportProvider::CallPostSetupCallback();
    TestSpiTransportProvider::ExchangeSemaphore(provider, semaphore);
    TEST_ASSERT_EQUAL_MESSAGE(messageBefore, provider->GetControlSignalMessage(), "The callback changed the control signal without a semaphore.");

    SpiTransportProvider * const instance = TestSpiTransportProvider::ExchangeInstance(nullptr);
    TestSpiTransportProvider::CallPostSetupCallback();
    TestSpiTransportProvider::ExchangeInstance(instance);
    TEST_ASSERT_EQUAL(messageBefore, provider->GetControlSignalMessage());
}
