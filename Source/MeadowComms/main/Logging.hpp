/*
 *  Logging.hpp
 *
 *  The Logging class provides the functionality necessary to
 *  setup and run the logging system.
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
#include "Gpio.hpp"
#include "Esp32Messaging.hpp"

//#define USE_MACRO 1
//#undef USE_MACRO

#define LOGGING_ENABLED 1
//#undef LOGGING_ENABLED

#if LOGGING_ENABLED
    #ifdef USE_MACRO
        #define INFORMATION_MESSAGE(condition, format, ...) ESP_LOGI(__func__, format, ##__VA_ARGS__);
        #define INFORMATION_HEX_BUFFER(buffer, bufferLength) ESP_LOG_BUFFER_HEXDUMP(__func__, buffer, bufferLength, ESP_LOGI);
        #define ERROR_MESSAGE(format, ...) ESP_LOGE(__func__, format, ##__VA_ARGS__);
        #define ERROR_HEX_BUFFER(buffer, bufferLength) ESP_LOG_BUFFER_HEXDUMP(__func__, buffer, bufferLength, ESP_LOGE);
    #else
        #define INFORMATION_MESSAGE(condition, format, ...) \
            do { \
                if (condition) Logging::Information(__func__, format, ##__VA_ARGS__); \
            } while (false)
        #define INFORMATION_HEX_BUFFER(condition, buffer, bufferLength) \
            do { \
                if (condition) Logging::Information(__func__, buffer, bufferLength); \
            } while (false)
        #define ERROR_MESSAGE(condition, format, ...) \
                do { \
                    if (condition) Logging::Information(__func__, buffer, bufferLength); \
                } while (false)
        #define ERROR_HEX_BUFFER(condition, buffer, bufferLength) \
                do { \
                    if (condition) Logging::Error(__func__, buffer, bufferLength); \
                } while (false)
    #endif
#else
    #define INFORMATION_MESSAGE(format, ...);
    #define INFORMATION_HEX_BUFFER(buffer, bufferLength);
    #define ERROR_MESSAGE(format, ...);
    #define ERROR_HEX_BUFFER(buffer, bufferLength);
#endif

/*
 *  @brief The Logging class provides the functionality necessary to setup and run the logging system.
 */
class Logging
{
private:
    /**
     *  UART Port number for the logging output.
     */
    static const uart_port_t LOGGING_UART_PORT_NUMBER = UART_NUM_0;

    /**
     *  Number of bytes in the Rx buffer.
     */
    static const int RX_BUFFER_SIZE = 1024;

    /**
     *  Number of bytes in the Rx buffer.
     */
    static const int TX_BUFFER_SIZE = 1024;

public:
    /*
     *  Default constructor for the logging system.
     */
    Logging();

    /*
     *  Default destructor for the logging system.
     */
    ~Logging();

    static void Setup();

    /*
     *  Process the request to output an informational message to the logging system.
     */
    static void Information(char *, char *, ...);

    /*
     *  Process the request to output an error message to the logging system
     */
    static void Error(char *, char *, ...);

    /*
     *  Decode the message structure and output the components.
     */
    static void DumpMessage(const char *, const Message *);
};


#endif /* MAIN_LOGGING_HPP_ */
