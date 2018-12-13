#pragma once

#define MSG_ACK                 (0)
#define MSG_NAK                 (1)

//
//  Error codes.
//
#define ERR_COMPLETED_OK                    (0)         /* Everything went OK */
#define ERR_CRC_ERROR                       (1)         /* CRC error, resend last packet */
#define ERR_RESTART                         (2)         /* Irrecoverable error, start again */
#define ERR_FAILURE                         (3)         /* General failure code. */
#define ERR_INVALID_INTERFACE               (4)
#define ERR_QUEUE_ERROR                     (5)
#define ERR_INVALID_PACKET                  (6)
#define ERR_INVALID_HEADER                  (7)
#define ERR_UNEXPECTED_DATA                 (8)
#define ERR_MISSING_END_OF_FRAME_MARKER     (9)
#define ERR_WIFI_ALREADY_STARTED            (10)
#define ERR_INVALID_SSID                    (11)
#define ERR_INVALID_PASSWORD                (12)
#define ERR_WIFI_DISCONNECTED               (13)