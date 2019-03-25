/*
 *  Logging.cpp
 *
 *      The Logging class provides the functionality necessary to
 *      setup and run the logging system.
 */

#include "Logging.hpp"


Logging::Logging()
{

}

Logging::~Logging()
{

}

/*
 * ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Setup the logging system.
 *
 *      The ESP32 should be set up to use custom logging (use 'make menuconfig' to do this) with
 *      the Tx pin set to GPIO22 and the Rx pin set to some other value (21 is used at the moment).
 *      This method then sets up the UART (should be UART0) so that the logging system can output
 *      data using this port.
 */
void Logging::Setup()
{
    const uart_config_t loggingUARTConfig =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(LOGGING_UART_PORT_NUMBER, &loggingUARTConfig);
    uart_set_pin(LOGGING_UART_PORT_NUMBER, Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //
    //  We won't use a buffer for sending data.
    //
    uart_driver_install(LOGGING_UART_PORT_NUMBER, RX_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    ESP_LOGI(__func__, "Logging redirected to UART_NUM_0 on pins %d (Tx), %d (Rx)", Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN);
}
