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

//
//  Company identifier is for Espressif (0x02e5).
//
const char *meadow_a_json = R"(
{
    "deviceName":"MeadowA",
    "companyIdentifier": 741,
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

//
//  Company ID is for Baidu (0x011c).
//
const char *meadow_b_json = R"(
{
    "deviceName":"MeadowB",
    "companyIdentifier": 284,
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

//
//  Meadow device with no company identifier.
//
const char *meadow_c_json = R"(
{
    "deviceName":"MeadowC",
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

//
//  Meadow device name MeadowF7 with company identifier Espressif (0x02e5).
//
//  This JSON represents the configuration used in the Bluetooth_Basics managed application.
//
const char *meadow_f7_json = R"({
    "deviceName":"MeadowF7",
    "companyIdentifier":741,
    "services": [
        {
            "name":"GattService",
            "uuid":253,
            "characteristics": [
                {   
                    "uuid":"016e99d6-8a61-11eb-8dcd-0242ac1300ff",
                    "permission":17,
                    "props":10,
                    "len":4
                },
                {
                    "uuid":"017e99d6-8a61-11eb-8dcd-0242ac1300aa",
                    "permission":1,
                    "props":2,
                    "len":4
                },
                {
                    "uuid":"018e99d6-8a61-11eb-8dcd-0242ac1300bb",
                    "permission":17,
                    "props":10,
                    "len":4
                },
                {
                    "uuid":"019e99d6-8a61-11eb-8dcd-0242ac1300cc",
                    "permission":17,
                    "props":10,
                    "len":256
                }
            ]
        }
    ]
})";

//
//  The JSON below causes a problem with the Descriptor processing, error:
// I (3121) BTDM_INIT: BT controller compile version [3bf7ef0]
// I (3121) BTDM_INIT: Bluetooth MAC: 84:0d:8e:18:5b:0a
// I (3131) phy_init: phy_version 4840,02e0d70,Sep  2 2024,19:39:07
// E (3531) BT_GATT: Error in gatts_add_char_desc_value_check, line=1611,attribute actual length should not be larger than max length
//
//
//  Company ID is for Baidu (0x011c).
//
// const char *meadow_b_json = R"(
// {
//     "deviceName":"MeadowB",
//     "companyIdentifier": 284,
//     "services": [
//         {
//             "name": "Service A",
//             "uuid": 254,
//             "characteristics": [
//                 {
//                     "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
//                     "permission": 17,
//                     "props": 10,
//                     "len": 4,
//                     "descriptors": [
//                         {
//                         "uuid": 10498,
//                         "permission": 1,
//                         "len": 2,
//                         "value": 0
//                         }
//                     ]
//                 },
//                 {
//                     "uuid": 10755,
//                     "permission": 2,
//                     "props": 16,
//                     "len": 4
//                 }
//             ]
//         }
//     ]
// })";

#endif // _TEST_BLUETOOTH_HPP_