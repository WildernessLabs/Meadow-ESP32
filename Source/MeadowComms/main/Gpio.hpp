/*
 *      Gpio.hpp
 *
 *      Constants defining the pins on the ESP32 that are
 *      available to Meadow.
 */

#ifndef _GPIO_HPP_
#define _GPIO_HPP_

#include "esp_system.h"

namespace Gpio
{
    /*
     *      Blinky pin, note that this is the same pin that is used
     *      for the UART Tx pin for the logging output.
     */
    const int BLINKY_PIN = GPIO_NUM_22;

    /*
     *      UART Tx pin for the logging output.
     */
    const int LOGGING_TX_PIN = GPIO_NUM_22;

    /*
     *      UART Rx pin for the logging output.
     */
    const int LOGGING_RX_PIN = GPIO_NUM_21;

    /*
     *      GPIO pin used to indicate to the STM32 that there is data
     *      waiting for transmission.
     */
    const gpio_num_t BOOT_PIN = GPIO_NUM_0;

    /*
     *      GPIO pin connected to UART0 Tx line.
     */
    const gpio_num_t UART0_RX_PIN = GPIO_NUM_3;

    /*
     *      GPIO pin connected to UART0 Rx line.
     */
    const gpio_num_t UART0_TX_PIN = GPIO_NUM_1;

    /*
     *      SPI MISO pin.
     */
    const gpio_num_t MISO_PIN = GPIO_NUM_19;

    /*
     *      SPI MOSI pin.
     */
    const gpio_num_t MOSI_PIN = GPIO_NUM_23;

    /*
     *      SPI Clock pin.
     */
    const gpio_num_t CLOCK_PIN = GPIO_NUM_18;

    /*
     *      SPI Chip Select pin.
     */
    const gpio_num_t CHIP_SELECT_PIN = GPIO_NUM_5;
}

#endif /* _GPIO_HPP_ */
