/**
 * @file SpiTransportProvider.hpp
 * @author Mark Stevens
 * @brief Hardware transport provider for messages between the ESP32 and the STM32 using the SPI interface.
 * @date 2019-03-15
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "driver/spi_slave.h"

#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"

/**
 *  @brief The SpiTransportProvider class provides the mechanism for sending and
 *  receiving messages between the ESP32 and the STM32.
 *
 *  Detailed description goes here.
 */
class SpiTransportProvider : public TransportProviderBase
{
    //
    //  ProcessInboundPacket is private production API; the tests need to drive it
    //  directly with synthetic packets to exercise the reassembly-buffer validation
    //  without a real STM32 peer.
    //
    friend class TestSpiTransportProvider;

private:
    /**
     *  @brief Stack size for the SPI task.
     */
    static constexpr uint32_t SPI_TASK_STACK_SIZE = 8192;

    /**
     *  @brief Stack size for the SPI synchronisation task.
     */
    static constexpr uint32_t SYNCHRONISATION_TASK_STACK_SIZE = 4096;

    /**
     *  @brief Possible states for the SPI transport layer.
     */
    enum SpiStates { Invalid, WaitingForPacket, AcknowledgingPacket, SendingPacketDetails, SendingPacket, WaitingForPacketAcknowledgement };

    /**
     *  @brief Pointer to the (only) instance of the SpiTransportProvider class.
     *
     *  This object is needed to allow the static callbacks access to the
     *  instance level variables and constants.
     */
    static SpiTransportProvider *_spiInstance;

    /**
     *  @brief Current state of the SPI transport layer.
     */
    static SpiStates _currentState;

    /**
     *  @brief The transaction used when none can be allocated.
     */
    static spi_slave_transaction_t _fallbackTransaction;

    /**
     *  @brief Buffer to hold a Message header worth of data.
     */
    static uint8_t *_rxBuffer;

    /**
     *  @brief Buffer to hold the data to be sent to the STM32.
     */
    static uint8_t *_txBuffer;

    /**
     *  @brief Size of a header / transport frame.
     */
    static uint32_t _headerFrameSize;

    /**
     *  @brief Message coming in from the STM32.
     */
    Message *_incomingMessage = nullptr;

    /**
     *  @brief The next message to be sent to the STM32.
     */
    Message *_outboundMessage = nullptr;

    /**
     *  @brief SPI slave transaction.
     *
     *  Note that it is assumed that this transaction is setup ready for
     *  the next use of the SPI interface.
     */
    spi_slave_transaction_t *_spiTransaction = nullptr;

    /**
     *  @brief Handle to the synchronisation task.
     */
    static TaskHandle_t _synchronisationTaskHandle;

    /**
     *  @brief Semaphore indicating if the system is trying to synchronise with the STM32.
     */
    SemaphoreHandle_t _synchronisationSemaphore = nullptr;

    /**
     *  @brief SPI Post Transmission callback
     *
     *  Note this this method is static as it is used as a callback
     *  in some C code.
     */
    static void SpiPostTransmissionCallback(spi_slave_transaction_t *transaction);

    /**
     *  @brief SPI Setup Complete Callback
     *
     *  Note this this method is static as it is used as a callback
     *  in some C code.
     */
    static void SpiPostSetupCallback(spi_slave_transaction_t *);

    /**
     *  @brief Send an ACK or NAK with the specified code.
     *
     *  The transaction is self-contained: it refers only to _txBuffer and _rxBuffer, so
     *  message can be released once this returns (issue #235).
     */
    spi_slave_transaction_t *CreateAcknowledgementTransaction(Message *message, StatusCodes::StatusCodes code, uint8_t messageType);

    /**
     *  @brief FreeRTOS task handler for this class.
     */
    static void Task(void *);

    /**
     *  @brief FreeRTOS task handler for the synchronisation task.
     */
    static void SynchronisationTask(void *);

    /**
     *  @brief Process incoming messages.
     */
    spi_slave_transaction_t *ProcessMessages(spi_slave_transaction_t *);

    /**
     *  @brief Convert the current state into a string message for debugging.
     */
    const char *SpiStateString(SpiStates state);

    /**
     *  @brief Check to see if the specified buffer looks to contain valid
     *  (non-zero) data.
     */
    bool MessageContainsData(const uint8_t *, uint32_t);

    /**
     *  @brief Check to see if the state we are in indicates that we should be
     *  receiving data from the STM32.
     */
    bool SendingData();

    /**
     *  @brief Process an inbound transport request.
     */
    spi_slave_transaction_t *ProcessTransportMessage(Message *request);

    /**
     *  @brief Work out what a finished transaction means in the current state and prepare the next one.
     */
    spi_slave_transaction_t *HandleTransaction(spi_slave_transaction_t *transaction);

    /**
     *  @brief Put the state machine back in its starting state after finding it in a state it does not know.
     */
    spi_slave_transaction_t *RecoverFromUnknownState(spi_slave_transaction_t *transaction);

    /**
     *  @brief Process the newly received packet of data and return transaction to be used as the response.
     */
    spi_slave_transaction_t *ProcessInboundPacket(Message *packet);

    /**
     *  @brief Start to process incoming messages (i.e. move from the idle state
     *  into one where the ESP32 is sending or receiving data).
     */
    spi_slave_transaction_t *PacketReceived(spi_slave_transaction_t *transaction);

    /**
     *  @brief Implement the incoming message header acknowledgement process.
     */
    spi_slave_transaction_t *PacketAcknowledged(spi_slave_transaction_t *transaction);

    /**
     *  @brief Implement the out-bound message details process.
     */
    spi_slave_transaction_t *PacketDetailsSent(spi_slave_transaction_t *transaction);

    /**
     *  @brief Implement processing of the incoming message header acknowledgement messages.
     */
    spi_slave_transaction_t *PacketSent(spi_slave_transaction_t *transaction);

    /**
     *  @brief Implement processing of the out-bound message body acknowledgement.
     */
    spi_slave_transaction_t *PacketAcknowledgementReceived(spi_slave_transaction_t *transaction);

    /**
     *  @brief Setup a new transaction using the information in the message.
     */
    spi_slave_transaction_t *CreateSpiTransaction(Message *message);

    /**
     *  @brief Create a transaction that sends an acknowledgement that is not for a particular message.
     */
    spi_slave_transaction_t *CreateTransportAcknowledgement(StatusCodes::StatusCodes code, uint8_t messageType);

    /**
     *  @brief Clear the data buffers used to send and receive data.
     */
    void ClearTransmitAndReceiveBuffers();

    /**
     *  @brief Free any memory allocated for the transaction but keep the
     *  Tx and Rx buffers.
     *
     *  @param transaction
     *      Pointer to the transaction (on the heap), taken by reference so the
     *      caller's pointer is nullified along with the storage it points to.
     */
    void DeleteTransaction(spi_slave_transaction_t *&transaction);

    /**
     *  @brief Update the packet information in the message to prepare the message for the next transmission.
     */
    void UpdatePacketInformation(Message *message);

public:
    /**
     *  @brief Name of this component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor.
     */
    SpiTransportProvider();

    /**
     *  @brief Default destructor.
     */
    virtual ~SpiTransportProvider();

    /*
     *  @brief Setup the SPI transport class.
     */
    void Setup(MessageDispatcher *messageDispatcher);
};
