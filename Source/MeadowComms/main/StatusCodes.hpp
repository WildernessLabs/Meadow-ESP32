/*
 * StatucCodes.hpp
 *
 *  Created on: 27 Jan 2019
 *      Author: markstevens
 */

#ifndef MAIN_STATUSCODES_HPP_
#define MAIN_STATUSCODES_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "esp_app_trace.h"
#include "string.h"
#include "nvs_flash.h"


/*
 *      Possible acknowledgement types.
 */
enum MessageAcknowledgement : uint8_t
{
    MSG_ACK = 0,
    MSG_NAK = 1
};
namespace StatusCodes
{
    /*
     *      Status and error codes for the various function calls.
     */
    enum StatusCode : uint32_t
    {
        CompletedOK = 0,
        CRCError = 1,
        Restart = 2,
        Failure = 3,
        InvalidInterface = 4,
        QueueError = 5,
        InvalidPacket = 6,
        InvalidHeader = 7,
        UnexpectedData =8,
        MissingEndOfFrameMarker = 9,
        WiFiAlreadyStarted = 10,
        InvalidSSID = 11,
        InvalidPassword = 12,
        WiFiDisconnected = 13
    };
}

#endif /* MAIN_STATUSCODES_HPP_ */
