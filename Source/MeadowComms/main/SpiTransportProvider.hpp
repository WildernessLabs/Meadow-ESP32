/*
 *      SpiTransportProvider.hpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#ifndef _SPI_TRANSPORT_PROVIDER_HPP_
#define _SPI_TRANSPORT_PROVIDER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_slave.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "Exceptions/MultipleInstancesException.hpp"
#include "TransportProviderBase.hpp"
#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Gpio.hpp"

class SpiTransportProvider : public TransportProviderBase
{
private:
    /*
     *      Maximum number of retries before the system gives up.
     */
    const int MAXIMUM_RETRY_COUNT = 3;

    /*
     *      Possible states for the SPI transport layer.
     */
    enum SpiStates { Invalid, Idle, AcknowledgingMessageHeader,
        WaitingForMessageBody, AcknowledingMessageBody, SendingMessageHeader, WaitingForMessageHeaderAcknowledgement,
        WantToSendMessageBody, WaitingForMessageBodyAcknowledgement, WantToSendTransportMessage };

    /*
     *      Pointer to the (only) instance of the SpiTransportProvider class.
     *
     *      This object is needed to allow the static callbacks access to the
     *      instance level variables and constants.
     */
    static SpiTransportProvider *_spiInstance;

    /*
     *      Current state of the SPI transport layer.
     */
    static SpiStates _currentState;

    /*
     *      Buffer to hold a Message header worth of data.
     */
    static uint8_t *_headerReceiveBuffer;

    /*
     *      Buffer to hold the data to be sent to the STM32.
     */
    static uint8_t *_headerTransmitBuffer;

    /*
     *      Buffer for the transport frame.
     */
    static uint8_t *_transportFrameBuffer;

    /*
     *      Size of a header / transport frame.
     */
    static uint32_t _headerFrameSize;

    /*
     *      Message coming in from the STM32.
     */
    Message *_incomingMessage = nullptr;

    /*
     *      The next message to be sent to the STM32.
     */
    Message *_outboundMessage = nullptr;

    /*
     *      SPI slave transaction.
     *
     *      Note that it is assumed that this transaction is setup ready for
     *      the next use of the SPI interface.
     */
    spi_slave_transaction_t *_spiTransaction = nullptr;

    /*
     *      Response code that will be used in any acknowledgement messages.
     */
    StatusCodes::StatusCodes _responseCode;

    /*
     *      Packet type that will be used in any acknowledgement messages.
     */
    uint8_t _messageType;

    /*
     *      Default destructor.
     */
    ~SpiTransportProvider();

    /*
     *      SPI Post Transmission callback
     *
     *      Note this this method is static as it is used as a callback
     *      in some C code.
     */
    static void SpiPostTransmissionCallback(spi_slave_transaction_t *);

    /*
     *      SPI Post setup callback.
     *
     *      Note this this method is static as it is used as a callback
     *      in some C code.
     */
    static void SpiPostSetupCallback(spi_slave_transaction_t *);

    /*
     *      Send an ACK or NAK with the specified code.
     */
    spi_slave_transaction_t *CreateAcknowledgementTransaction(Message *, StatusCodes::StatusCodes, uint8_t);

    /*
     *  Encode a header or transport message.
     */
    void EncodeHeaderMessage(spi_slave_transaction_t *, Message *);
    /*
     *      FreeRTOS task handler for this class.
     */
    static void Task(void *);

    spi_slave_transaction_t *ProcessMessages(spi_slave_transaction_t *);

    /*
     *  Convert the current state into a string message for debugging.
     */
    const char *SpiStateString(SpiStates);

    /*
     *      Delete the SPI transaction and clear up and buffers;
     */
    void DeleteSpiTransaction(spi_slave_transaction_t *);

    /*
     *      Clear the _currentHeader variable and free any allocated
     *      heap memory.
     */
    void DeleteMessageAndReleaseHeapStorage(Message *);

    spi_slave_transaction_t *ProcessTransportMessage();
    /*
     *      Start to process incoming messages (i.e. move from the idle state
     *      into one where the ESP32 is sending or receiving data).
     */
    spi_slave_transaction_t *ProcessIdleState(spi_slave_transaction_t *);

    /*
     *      Implement the incoming message header reception process.
     */
    spi_slave_transaction_t *ProcessWaitingForMessageHeaderState(spi_slave_transaction_t *);

    /*
     *      Implement the incoming message header acknowledgement process.
     */
    spi_slave_transaction_t *ProcessAcknowledgingMessageHeaderState(spi_slave_transaction_t *);

    /*
     *      Implement the incoming message body reception process.
     */
    spi_slave_transaction_t *ProcessWaitingForMessageBodyState(spi_slave_transaction_t *);

    /*
     *      Implement the incoming message body acknowledgement process.
     */
    spi_slave_transaction_t *ProcessAcknowledingMessageBodyState(spi_slave_transaction_t *);

    /*
     *      Implement the out-bound message header process.
     */
    spi_slave_transaction_t *ProcessSendingMessageHeaderState(spi_slave_transaction_t *);

    /*
     *      Implement processing of the incoming message header acknowledgement messages.
     */
    void ProcessWaitingForMessageHeaderAcknowledgementState();

    /*
     *      Implement the out-bound message body process.
     */
    spi_slave_transaction_t *ProcessWantToSendMessageBodyState(spi_slave_transaction_t *);

    /*
     *      Implement processing of the out-bound message body acknowledgement.
     */
    void ProcessWaitingForMessageBodyAcknowledgementState();

    /*
     *      Implement the out-bound transport message process.
     */
    void ProcessWantToSendTransportMessageState();

    /*
     *      Create a new SPI slave transmission object with zero filled
     *      buffers of the requested size.
     */
    spi_slave_transaction_t *CreateSpiPayloadTransaction(uint32_t);

    /*
     *      Setup a new transaction using the _headerReceiveBuffer and
     *      _headerTransmitBuffer.
     */
    spi_slave_transaction_t *CreateSpiHeaderTransaction();

    void ClearTransmitAndReceiveBuffers(uint8_t *, uint8_t *, uint32_t);

    /*
     *      Free any memory allocated for the transaction and its Tx and Rx buffers.
     */
    void DeleteMessageTransaction(spi_slave_transaction_t *);

    /*
     *      Free any memory allocated for the transaction but keep the
     *      Tx and Rx buffers.
     */
    void DeleteHeaderTransaction(spi_slave_transaction_t *);

public:
    /*
     *      Default constructor.
     */
    SpiTransportProvider();

    /*
     *      Setup the SPI transport class.
     */
    void Setup(MessageDispatcher *);

    /*
     *      Test method that will simply pass the string through
     *      the MessageDispatcher to verify that the message
     *      passing between layers is working.
     */
    void Echo(const char *);
};

#endif /* _SPI_TRANSPORT_PROVIDER_HPP_ */
