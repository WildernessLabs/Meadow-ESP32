/*
 *      SpiTransportProvider.hpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#ifndef _SPI_TRANSPORT_PROVIDER_HPP_
#define _SPI_TRANSPORT_PROVIDER_HPP_

#include "freertos/FreeRTOS.h"
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
     *      Possible states for the SPI transport layer.
     */
    enum SpiStates { Invalid, Idle, AcknowledgingMessageHeader,
        WaitingForMessageBody, AcknowledingMessageBody, WantToSendMessageHeader, WaitingForMessageHeaderAcknowledgement,
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
    static uint8_t *_headerBuffer;

    /*
     *      Buffer to hold the data to be sent to the STM32.
     */
    static uint8_t *_transmitBuffer;

    /*
     *      Buffer for the transport frame.
     */
    static uint8_t *_transportFrameBuffer;

    /*
     *      Size of a header / transport frame.
     */
    static uint32_t _headerFrameSize;

    /*
     *      Current message header being processed.  The STM32 will send
     *      the message header before the message body.  This is done so
     *      that the ESP32 can allocate enough memory for the data being
     *      sent.
     *
     *      This variable holds the message header that has been sent.
     */
    Message *_currentHeader = nullptr;

    /*
     *      The next message to be sent to the STM32.
     */
    Message *_outboundMessage = nullptr;

    /*
     *      Handle for the System request handler task.
     */
    static TaskHandle_t _taskHandle;

    /*
     *      Response code that will be used in any acknowledgement messages.
     */
    StatusCodes::StatusCodes _responseCode;

    /*
     *      Packet type that will be used in any acknowledgement messages.
     */
    uint8_t _packetType;

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
    spi_slave_transaction_t *AcknowledgeMessage(Message *, StatusCodes::StatusCodes, uint8_t);

    /*
     *      FreeRTOS task handler for this class.
     */
    static void Task(void *);

    static void IncomingSpiMessageTask(void *);

    /*
     *      Clear the _currentHeader variable and free any allocated
     *      heap memory.
     */
    void ClearCurrentHeader();

    /*
     *      Start to process incoming messages (i.e. move from the idle state
     *      into one where the ESP32 is sending or receiving data).
     */
    void ProcessIdleState();

    /*
     *      Implement the incoming message header reception process.
     */
    void ProcessWaitingForMessageHeaderState();

    /*
     *      Implement the incoming message header acknowledgement process.
     */
    void ProcessAcknowledgingMessageHeaderState();

    /*
     *      Implement the incoming message body reception process.
     */
    void ProcessWaitingForMessageBodyState();

    /*
     *      Implement the incoming message body acknowledgement process.
     */
    void ProcessAcknowledingMessageBodyState();

    /*
     *      Implement the out-bound message header process.
     */
    void ProcessWantToSendMessageHeaderState();

    /*
     *      Implement processing of the incoming message header acknowledgement messages.
     */
    void ProcessWaitingForMessageHeaderAcknowledgementState();

    /*
     *      Implement the out-bound message body process.
     */
    void ProcessWantToSendMessageBodyState();

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
    spi_slave_transaction_t *SetupSpiSlaveTransaction(uint32_t, uint32_t);

    /*
     *      Free any memory allocated for the transaction and its Tx and Rx buffers.
     */
    void FreeUpTransactionHeapStorage(spi_slave_transaction_t *);

    /*
     *      ISR for the GPIO ISR.
     */
    static void Stm32IsrHandler(void *);

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
