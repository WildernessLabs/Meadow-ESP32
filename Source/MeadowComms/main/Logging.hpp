/*
 *      Logging.hpp
 *
 *      The Logging class provides the functionality necessary to
 *      setup and run the logging system.
 */

#ifndef MAIN_LOGGING_HPP_
#define MAIN_LOGGING_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

/*
 *      The Logging class provides the functionality necessary to
 *      setup and run the logging system.
 */
class Logging
{
private:
    /*
     *      UART Tx pin for the logging output.
     */
    static const int LOGGING_TX_PIN = GPIO_NUM_22;

    /*
     *      UART Rx pin for the logging output.
     */
    static const int LOGGING_RX_PIN = GPIO_NUM_21;

    /*
     *      UART Port number for the logging output.
     */
    static const uart_port_t LOGGING_UART_PORT_NUMBER = UART_NUM_0;

    /*
     *      Number of bytes in the Rx buffer.
     */
    static const int RX_BUFFER_SIZE = 1024;
    /*
     *      Number of bytes in the Rx buffer.
     */
    static const int TX_BUFFER_SIZE = 1024;


public:
    Logging();
    ~Logging();

    static void Setup();
};


#endif /* MAIN_LOGGING_HPP_ */
