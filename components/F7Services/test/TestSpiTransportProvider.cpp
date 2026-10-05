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
     * @brief Call the private UpdatePacketInformation method.
     */
    static void CallUpdatePacketInformation(SpiTransportProvider *provider, Message *message)
    {
        provider->UpdatePacketInformation(message);
    }

    /**
     * @brief The provider's outbound queue.
     */
    static QueueHandle_t OutboundQueue()
    {
        return (SpiTransportProvider::_outboundMessageQueueHandle);
    }

    /**
     * @brief Give the provider an outbound queue and mutex if it has none.
     *
     *  The queue is made by the message dispatcher in the firmware, and these tests have none.
     *
     * @return true if they were made here, and so are to be released with ReleaseOutboundQueue.
     */
    static bool EnsureOutboundQueue()
    {
        if (SpiTransportProvider::_outboundMessageQueueHandle != nullptr)
        {
            return (false);
        }
        SpiTransportProvider::_outboundMessageQueueHandle = xQueueCreate(8, sizeof(Message *));
        SpiTransportProvider::_outboundMessageQueueMutex = xSemaphoreCreateMutex();
        TEST_ASSERT_NOT_NULL(SpiTransportProvider::_outboundMessageQueueHandle);
        TEST_ASSERT_NOT_NULL(SpiTransportProvider::_outboundMessageQueueMutex);
        return (true);
    }

    /**
     * @brief Release the queue and mutex made by EnsureOutboundQueue, and any message still on the queue.
     */
    static void ReleaseOutboundQueue()
    {
        Message *queued = nullptr;
        while (xQueueReceive(SpiTransportProvider::_outboundMessageQueueHandle, &queued, 0) == pdPASS)
        {
            Message::DeleteMessage(queued);
        }
        vQueueDelete(SpiTransportProvider::_outboundMessageQueueHandle);
        vSemaphoreDelete(SpiTransportProvider::_outboundMessageQueueMutex);
        SpiTransportProvider::_outboundMessageQueueHandle = nullptr;
        SpiTransportProvider::_outboundMessageQueueMutex = nullptr;
    }

    /**
     * @brief Take the message with a message ID off the outbound queue, putting back the others.
     *
     * @param messageId ID of the message wanted.
     *
     * @return The message, nullptr if it is not on the queue.
     */
    static Message *TakeQueuedMessage(const uint32_t messageId)
    {
        Message *found = nullptr;
        const UBaseType_t waiting = uxQueueMessagesWaiting(SpiTransportProvider::_outboundMessageQueueHandle);
        for (UBaseType_t index = 0; index < waiting; index++)
        {
            Message *queued = nullptr;
            if (xQueueReceive(SpiTransportProvider::_outboundMessageQueueHandle, &queued, 0) == pdPASS)
            {
                if ((found == nullptr) && (queued->MessageID == messageId))
                {
                    found = queued;
                }
                else
                {
                    xQueueSendToBack(SpiTransportProvider::_outboundMessageQueueHandle, &queued, 0);
                }
            }
        }
        return (found);
    }

    /**
     * @brief Call the private HandleTransaction method.
     */
    static spi_slave_transaction_t *CallHandleTransaction(SpiTransportProvider *provider, spi_slave_transaction_t *transaction)
    {
        return (provider->HandleTransaction(transaction));
    }

    /**
     * @brief Call the private ProcessTransportMessage method.
     */
    static spi_slave_transaction_t *CallProcessTransportMessage(SpiTransportProvider *provider, Message *request)
    {
        return (provider->ProcessTransportMessage(request));
    }

    /**
     * @brief Put the private, static state machine in a state, which need not be one it has.
     */
    static void SetState(const int state)
    {
        SpiTransportProvider::_currentState = static_cast<SpiTransportProvider::SpiStates>(state);
    }

    /**
     * @brief Read the private, static state.
     */
    static int GetState()
    {
        return (static_cast<int>(SpiTransportProvider::_currentState));
    }

    /**
     * @brief The state the machine waits for a packet in.
     */
    static int WaitingState()
    {
        return (static_cast<int>(SpiTransportProvider::WaitingForPacket));
    }

    /**
     * @brief The state the machine acknowledges a packet in.
     */
    static int AcknowledgingState()
    {
        return (static_cast<int>(SpiTransportProvider::AcknowledgingPacket));
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
    TEST_ASSERT_TRUE(Encoders::EncodeMessage(message, expected, Message::MAXIMUM_SPI_FRAME_SIZE, false));
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
        TEST_ASSERT_TRUE(Encoders::EncodeMessage(acknowledgement, static_cast<uint8_t *>(transaction->rx_buffer), Message::MAXIMUM_SPI_FRAME_SIZE, true));
        //
        //  The driver reports the number of bits it received, which is the size of a header frame for an acknowledgement.
        //
        transaction->trans_len = TestSpiTransportProvider::GetHeaderFrameSize() * 8;
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

/**
 * @brief Make a message to send with a payload of a length, every byte of it a recognisable value.
 *
 * @param messageId ID for the message.
 * @param payloadLength Number of bytes in the payload.
 *
 * @return The message, which the queue or the caller releases.
 */
static Message *CreateOutboundMessage(const uint32_t messageId, const uint32_t payloadLength)
{
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 0xc3, payloadLength);
    Message *message = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::System, 17, StatusCodes::CompletedOk, messageId, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(message);
    return (message);
}

TEST_CASE("A payload longer than the protocol maximum is not queued for the STM32", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    const uint32_t LONG_ID = 0x27500001;
    const uint32_t LONGEST_ID = 0x27500002;
    const uint32_t HUGE_ID = 0x27500003;
    const bool madeQueue = TestSpiTransportProvider::EnsureOutboundQueue();

    //
    //  A payload of the maximum goes through as it is, and one byte more goes without its payload and
    //  with a failure status, the function and message ID it answers kept.
    //
    TEST_ASSERT_TRUE(provider->QueueMessageForStm32(CreateOutboundMessage(LONGEST_ID, Message::MAXIMUM_PAYLOAD_LENGTH)));
    TEST_ASSERT_TRUE(provider->QueueMessageForStm32(CreateOutboundMessage(LONG_ID, Message::MAXIMUM_PAYLOAD_LENGTH + 1)));
    TEST_ASSERT_TRUE(provider->QueueMessageForStm32(CreateOutboundMessage(HUGE_ID, 70000)));

    Message *longest = TestSpiTransportProvider::TakeQueuedMessage(LONGEST_ID);
    TEST_ASSERT_NOT_NULL(longest);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(Message::MAXIMUM_PAYLOAD_LENGTH, longest->PayloadLength, "A payload of the maximum length was changed.");
    TEST_ASSERT_NOT_NULL(longest->Payload);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::CompletedOk, longest->StatusCode);
    Message::DeleteMessage(longest);

    Message *refused = TestSpiTransportProvider::TakeQueuedMessage(LONG_ID);
    TEST_ASSERT_NOT_NULL(refused);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, refused->PayloadLength, "A payload one byte over the maximum was queued.");
    TEST_ASSERT_NULL(refused->Payload);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::Failure, refused->StatusCode);
    TEST_ASSERT_EQUAL_UINT32(17, refused->Function);
    TEST_ASSERT_EQUAL_UINT8(Esp32Interfaces::System, refused->Interface);
    Message::DeleteMessage(refused);

    Message *huge = TestSpiTransportProvider::TakeQueuedMessage(HUGE_ID);
    TEST_ASSERT_NOT_NULL(huge);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, huge->PayloadLength, "A payload of 70000 bytes was queued.");
    TEST_ASSERT_NULL(huge->Payload);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::Failure, huge->StatusCode);
    Message::DeleteMessage(huge);

    if (madeQueue)
    {
        TestSpiTransportProvider::ReleaseOutboundQueue();
    }
}

TEST_CASE("The packets of a payload are worked out in 32 bits", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();

    //
    //  The payload is never read here, only its length.  The first packet is 4061 bytes, and so is the
    //  next: worked out in 16 bits the remaining 65939 bytes wrapped to 429.
    //
    Message *message = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::System, 17, StatusCodes::CompletedOk, 1, nullptr, 0);
    TEST_ASSERT_NOT_NULL(message);
    message->PayloadLength = 70000;
    message->PacketOffset = 0;
    message->PacketLength = 0;

    TestSpiTransportProvider::CallUpdatePacketInformation(provider, message);
    TEST_ASSERT_EQUAL_UINT16(0, message->PacketOffset);
    TEST_ASSERT_EQUAL_UINT16(Message::MAXIMUM_PACKET_SIZE, message->PacketLength);
    TestSpiTransportProvider::CallUpdatePacketInformation(provider, message);
    TEST_ASSERT_EQUAL_UINT16(Message::MAXIMUM_PACKET_SIZE, message->PacketOffset);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(Message::MAXIMUM_PACKET_SIZE, message->PacketLength, "The second packet of a long payload was cut short.");

    //
    //  The packets of a payload of the maximum: two full ones and what is left.
    //
    message->PayloadLength = Message::MAXIMUM_PAYLOAD_LENGTH;
    message->PacketOffset = 0;
    message->PacketLength = 0;
    const uint16_t expected[] = {Message::MAXIMUM_PACKET_SIZE, Message::MAXIMUM_PACKET_SIZE, Message::MAXIMUM_PAYLOAD_LENGTH - (2 * Message::MAXIMUM_PACKET_SIZE)};
    uint32_t sent = 0;
    for (const uint16_t length: expected)
    {
        TestSpiTransportProvider::CallUpdatePacketInformation(provider, message);
        TEST_ASSERT_EQUAL_UINT16(sent, message->PacketOffset);
        TEST_ASSERT_EQUAL_UINT16(length, message->PacketLength);
        sent += length;
    }
    TEST_ASSERT_EQUAL_UINT32(Message::MAXIMUM_PAYLOAD_LENGTH, sent);

    //
    //  An offset past the end of the payload leaves nothing to send, rather than wrapping.
    //
    message->PayloadLength = 100;
    message->PacketOffset = 200;
    message->PacketLength = 0;
    TestSpiTransportProvider::CallUpdatePacketInformation(provider, message);
    TEST_ASSERT_EQUAL_UINT16(0, message->PacketLength);

    Message::DeleteMessage(message);
}

/**
 * @brief Make an outbound message of three packets, send its header and its first packet, and return the transaction for the first packet.
 *
 * @param provider Provider to send it with.
 * @param message Filled in with the outbound message, which the provider holds.
 *
 * @return The transaction whose acknowledgement is next.
 */
static spi_slave_transaction_t *SendFirstPacket(SpiTransportProvider *provider, Message **message)
{
    const uint32_t payloadLength = (2 * Message::MAXIMUM_PACKET_SIZE) + 10;
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 0x3c, payloadLength);
    *message = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::WiFi, 3, StatusCodes::CompletedOk, 99, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(*message);
    TestSpiTransportProvider::SetOutboundMessage(provider, *message);

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateSpiTransaction(provider, nullptr);
    return (TestSpiTransportProvider::CallPacketDetailsSent(provider, transaction));
}

/**
 * @brief Put an acknowledgement for the message in a transaction's receive buffer, as a frame of a length, sealed with a CRC over all of it.
 *
 * @param transaction Transaction to receive it.
 * @param frameLength Number of bytes the sender is taken to have sent.
 * @param messageType Type of the message: an Ack, or a Nak.
 */
static void ReceiveAcknowledgement(spi_slave_transaction_t *transaction, const uint32_t frameLength, const uint8_t messageType = MessageTypes::Ack)
{
    Message *acknowledgement = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, StatusCodes::CompletedOk, 99, nullptr, 0);
    TEST_ASSERT_NOT_NULL(acknowledgement);
    uint8_t *buffer = static_cast<uint8_t *>(transaction->rx_buffer);
    memset(buffer, 0, Message::MAXIMUM_SPI_FRAME_SIZE);
    TEST_ASSERT_TRUE(Encoders::EncodeMessage(acknowledgement, buffer, Message::MAXIMUM_SPI_FRAME_SIZE, true));
    Message::DeleteMessage(acknowledgement);
    Encoders::EncodeUInt32(0, buffer + Message::CRC_OFFSET);
    Encoders::EncodeUInt32(Encoders::CRC32(buffer, frameLength - Message::SPI_MESSAGE_OVERHEAD), buffer + Message::CRC_OFFSET);
    transaction->trans_len = frameLength * 8;
}

TEST_CASE("An acknowledgement is decoded from the number of bytes received", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();
    const bool madeQueue = TestSpiTransportProvider::EnsureOutboundQueue();

    //
    //  A frame of 64 bytes, longer than a header frame, with a CRC over all of it.  Decoded as a header frame
    //  of 32 bytes its CRC did not match and the acknowledgement was lost (issue #321).
    //
    Message *message = nullptr;
    spi_slave_transaction_t *transaction = SendFirstPacket(provider, &message);
    ReceiveAcknowledgement(transaction, 64);
    transaction = TestSpiTransportProvider::CallPacketAcknowledgementReceived(provider, transaction);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(message, TestSpiTransportProvider::GetOutboundMessage(provider), "An acknowledgement of the length received was not accepted.");
    TEST_ASSERT_EQUAL_UINT16(Message::MAXIMUM_PACKET_SIZE, message->PacketOffset);
    TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
    TestSpiTransportProvider::SetOutboundMessage(provider, nullptr);
    TestSpiTransportProvider::ResetState();
    Message::DeleteMessage(message);

    //
    //  A frame that is too short to hold a header is not an acknowledgement, whatever the buffer beyond it holds.
    //  The message goes back on the queue to be sent again.
    //
    transaction = SendFirstPacket(provider, &message);
    ReceiveAcknowledgement(transaction, TestSpiTransportProvider::GetHeaderFrameSize());
    transaction->trans_len = 16 * 8;
    transaction = TestSpiTransportProvider::CallPacketAcknowledgementReceived(provider, transaction);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_NULL_MESSAGE(TestSpiTransportProvider::GetOutboundMessage(provider), "A frame too short for a header was taken for an acknowledgement.");
    TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
    TestSpiTransportProvider::ResetState();

    if (madeQueue)
    {
        TestSpiTransportProvider::ReleaseOutboundQueue();
    }
}

/**
 * @brief Number of messages on the provider's outbound queue.
 */
static UBaseType_t QueuedMessageCount()
{
    return (uxQueueMessagesWaiting(TestSpiTransportProvider::OutboundQueue()));
}

TEST_CASE("An acknowledgement with no outbound message to acknowledge is ignored", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();
    const bool madeQueue = TestSpiTransportProvider::EnsureOutboundQueue();
    TestSpiTransportProvider::SetOutboundMessage(provider, nullptr);

    //
    //  An acknowledgement, a negative one, and a frame that is not a message at all, each arriving when the
    //  outbound message has already gone.  The first used to be passed to UpdatePacketInformation as a null
    //  message, and the others put a null message on the queue for the transport task to dereference (issue #233).
    //
    const uint8_t types[] = {MessageTypes::Ack, MessageTypes::Nak};
    for (const uint8_t type: types)
    {
        spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateSpiTransaction(provider, nullptr);
        ReceiveAcknowledgement(transaction, TestSpiTransportProvider::GetHeaderFrameSize(), type);
        const UBaseType_t queuedBefore = QueuedMessageCount();
        transaction = TestSpiTransportProvider::CallPacketAcknowledgementReceived(provider, transaction);
        TEST_ASSERT_NOT_NULL_MESSAGE(transaction, "No transaction was returned after an acknowledgement with nothing to acknowledge.");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(queuedBefore, QueuedMessageCount(), "An acknowledgement with no outbound message put something on the queue.");
        TEST_ASSERT_NULL(TestSpiTransportProvider::GetOutboundMessage(provider));
        TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
        TestSpiTransportProvider::ResetState();
    }

    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateSpiTransaction(provider, nullptr);
    memset(transaction->rx_buffer, 0xff, Message::MAXIMUM_SPI_FRAME_SIZE);
    transaction->trans_len = TestSpiTransportProvider::GetHeaderFrameSize() * 8;
    const UBaseType_t queuedBefore = QueuedMessageCount();
    transaction = TestSpiTransportProvider::CallPacketAcknowledgementReceived(provider, transaction);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(queuedBefore, QueuedMessageCount(), "A frame that could not be decoded, with no outbound message, put something on the queue.");
    TestSpiTransportProvider::CallDeleteTransaction(provider, transaction);
    TestSpiTransportProvider::ResetState();

    if (madeQueue)
    {
        TestSpiTransportProvider::ReleaseOutboundQueue();
    }
}

TEST_CASE("A state the machine does not know is recovered from", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();

    //
    //  The Invalid state, and the two values after the last of the five states the machine works in (the enumeration
    //  has room for eight), with a message half received and one half sent.  The result used to be a null transaction,
    //  which the task loop queued and so reset the board (issues #176 and #232).
    //
    const int unknownStates[] = {0, 6, 7};
    for (const int unknownState: unknownStates)
    {
        TestSpiTransportProvider::SetOutboundMessage(provider, Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::WiFi, 3, StatusCodes::CompletedOk, 7, nullptr, 0));
        TestSpiTransportProvider::SetIncomingMessage(provider, Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, 3, StatusCodes::CompletedOk, 8, nullptr, 0));
        TestSpiTransportProvider::SetState(unknownState);

        spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallCreateSpiTransaction(provider, nullptr);
        memset(transaction->rx_buffer, 0xff, Message::MAXIMUM_SPI_FRAME_SIZE);
        transaction->trans_len = TestSpiTransportProvider::GetHeaderFrameSize() * 8;
        spi_slave_transaction_t *next = TestSpiTransportProvider::CallHandleTransaction(provider, transaction);

        TEST_ASSERT_NOT_NULL_MESSAGE(next, "An unknown state returned no transaction.");
        TEST_ASSERT_EQUAL_INT_MESSAGE(TestSpiTransportProvider::WaitingState(), TestSpiTransportProvider::GetState(), "The state machine was not put back to waiting for a packet.");
        TEST_ASSERT_NULL_MESSAGE(TestSpiTransportProvider::GetOutboundMessage(provider), "The outbound message was kept in a state that was not known.");
        TEST_ASSERT_NULL_MESSAGE(TestSpiTransportProvider::GetIncomingMessage(provider), "The message being received was kept in a state that was not known.");
        TestSpiTransportProvider::CallDeleteTransaction(provider, next);
    }
    TestSpiTransportProvider::ResetState();
}

TEST_CASE("A transport request for a function that is not known is refused", CLASS_OF_TESTS)
{
    SpiTransportProvider *provider = EnsureProvider();
    ResetIncomingMessage();
    TestSpiTransportProvider::SetOutboundMessage(provider, nullptr);
    TestSpiTransportProvider::SetState(TestSpiTransportProvider::AcknowledgingState());

    //
    //  Used to return a null transaction, and so reset the board (issue #232).  It is now answered with a negative
    //  acknowledgement, like any other message the machine does not expect.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Transport, Esp32Interfaces::Transport, 0xfffe, StatusCodes::CompletedOk, 21, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);
    spi_slave_transaction_t *transaction = TestSpiTransportProvider::CallProcessTransportMessage(provider, request);
    Message::DeleteMessage(request);
    TEST_ASSERT_NOT_NULL_MESSAGE(transaction, "An unknown transport function returned no transaction.");

    uint8_t messageType = 0;
    uint32_t statusCode = 0;
    DecodeAndFreeAcknowledgement(transaction, &messageType, &statusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Nak, messageType);
    TEST_ASSERT_EQUAL(StatusCodes::UnexpectedData, statusCode);
    TEST_ASSERT_EQUAL_INT(TestSpiTransportProvider::AcknowledgingState(), TestSpiTransportProvider::GetState());

    TestSpiTransportProvider::SetOutboundMessage(provider, nullptr);
    TestSpiTransportProvider::ResetState();
}
