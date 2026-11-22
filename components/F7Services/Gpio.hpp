/**
 * @file Gpio.hpp
 * @author Mark Stevens
 * @brief Constants defining the pins on the ESP32 that are available to Meadow.
 * @date 2019-03-25
 *
 * @copyright Copyright (c) 2019-2026 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "esp_system.h"
#include "driver/gpio.h"

/**
 * The Gpio namespace contains the definitions of the constants that define the GPIO
 * pins along with their purpose.
 *
 * The ESP32 pins are the ones wired to the STM32 on the Meadow boards.  The ESP32-S3 pins
 * are for the emulator build (build.sh --emulator) only: esp-emu has no ESP32, and several
 * ESP32 pins do not exist on the S3 (GPIO 23) or are its USB pins (GPIO 19 and 20).
 */
namespace Gpio
{
#if CONFIG_IDF_TARGET_ESP32
    /**
     *  UART Tx pin for the logging output.
     */
    const gpio_num_t LOGGING_TX_PIN = GPIO_NUM_1;

    /**
     *  UART Rx pin for the logging output.
     *
     *  Note that the pin used here is not actually connected to
     *  anything but the pin is necessary to setup the logging UART.
     */
    const gpio_num_t LOGGING_RX_PIN = GPIO_NUM_21;

    /**
     *  ESP32 Boot pin.
     */
    const gpio_num_t BOOT_PIN = GPIO_NUM_0;

    /**
     *  Usual GPIO UART0 Rx line.
     *
     *  This pin is not used in the application for UART communication as
     *  it is being used for signalling.
     */
    const gpio_num_t UART0_RX_PIN = GPIO_NUM_3;

    /**
     *  GPIO pin connected to UART0 Tx line.
     */
    const gpio_num_t UART0_TX_PIN = GPIO_NUM_1;

    /**
     *  SPI MISO pin.
     */
    const gpio_num_t MISO_PIN = GPIO_NUM_19;

    /**
     *  SPI MOSI pin.
     */
    const gpio_num_t MOSI_PIN = GPIO_NUM_23;

    /**
     *  SPI Clock pin.
     */
    const gpio_num_t CLOCK_PIN = GPIO_NUM_18;

    /**
     *  SPI Chip Select pin.
     */
    const gpio_num_t CHIP_SELECT_PIN = GPIO_NUM_5;
#elif CONFIG_IDF_TARGET_ESP32S3
    /**
     *  UART Tx pin for the logging output: UART0's default Tx pad on the S3.
     */
    const gpio_num_t LOGGING_TX_PIN = GPIO_NUM_43;

    /**
     *  UART Rx pin for the logging output: UART0's default Rx pad on the S3.
     */
    const gpio_num_t LOGGING_RX_PIN = GPIO_NUM_44;

    /**
     *  ESP32-S3 Boot pin.
     */
    const gpio_num_t BOOT_PIN = GPIO_NUM_0;

    /**
     *  UART0 Rx line.
     */
    const gpio_num_t UART0_RX_PIN = GPIO_NUM_44;

    /**
     *  UART0 Tx line.
     */
    const gpio_num_t UART0_TX_PIN = GPIO_NUM_43;

    /**
     *  SPI MISO pin (the S3's FSPIQ pad).
     */
    const gpio_num_t MISO_PIN = GPIO_NUM_13;

    /**
     *  SPI MOSI pin (the S3's FSPID pad).
     */
    const gpio_num_t MOSI_PIN = GPIO_NUM_11;

    /**
     *  SPI Clock pin (the S3's FSPICLK pad).
     */
    const gpio_num_t CLOCK_PIN = GPIO_NUM_12;

    /**
     *  SPI Chip Select pin (the S3's FSPICS0 pad).
     */
    const gpio_num_t CHIP_SELECT_PIN = GPIO_NUM_10;
#else
#error "Gpio.hpp has no pin map for this target."
#endif
} // namespace Gpio
