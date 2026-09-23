/**
 * @file TestBluetoothServiceJson.hpp
 * @author Mark Stevens
 *
 * @brief JSON encoding of the Bluetooth service configuration.
 *
 * @copyright Copyright (c) 2025
 */
#ifndef _TEST_BLUETOOTH_SERVICE_JSON_HPP_
#define _TEST_BLUETOOTH_SERVICE_JSON_HPP_

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
//  The firmware does not add a Client Characteristic Configuration descriptor (0x2902) to
//  a notifying characteristic, so one is declared here.  Without it a client cannot
//  subscribe: CoreBluetooth reports "attribute not found" and never delivers the value.
//  The descriptor is two bytes, and 0 means no subscription.
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
                    "props": 26,
                    "len": 4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 17,
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

//
//  Company ID is for Baidu (0x011c).
//
const char *meadow_d_json = R"(
{
    "deviceName":"MeadowD",
    "companyIdentifier": 284,
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 18,
                    "len": 4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 17,
                        "len": 2,
                        "value": 0
                        },
                        {
                        "uuid": 10497,
                        "permission": 1,
                        "len": 20,
                        "value": "Test characteristic"
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

#endif // _TEST_BLUETOOTH_SERVICE_JSON_HPP_