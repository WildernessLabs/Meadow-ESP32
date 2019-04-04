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
    enum SpiStates { Invalid, Idle, WaitingForMessageHeader, WaitingForMessageBody };

    /*
     *      Pointer to the (only) instance of the SpiTransportProvider class.
     *
     *      This object is needed to allow the static callbacks access to the
     *      instance level variables and constants.
     */
    static SpiTransportProvider *_spiInstance;

    /*
     *      SPI Transaction.
     */
    static spi_slave_transaction_t _transaction;

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
     *      Handle for the System request handler task.
     */
    static TaskHandle_t _taskHandle;

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

    static void SendErrorResponse(StatusCodes::StatusCodes);

    /*
     *      FreeRTOS task handler for this class.
     */
    static void Task(void *);

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
