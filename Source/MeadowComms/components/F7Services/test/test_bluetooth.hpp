/**
 * @file test_bluetooth.hpp
 * @author Mark Stevens
 * @brief Hold defintions used by the Bluetooth tests.
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024
 */
#ifndef _TEST_BLUETOOTH_HPP_
#define _TEST_BLUETOOTH_HPP_

#include "sdkconfig.h"

#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include <string.h>

const char *json_test = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

const char *json_test2 = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 1,
                        "len": 2,
                        "value": 0
                        }
                    ]
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

#endif // _TEST_BLUETOOTH_HPP_