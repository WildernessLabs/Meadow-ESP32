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
//  The notifying characteristics declare no Client Characteristic Configuration descriptor
//  (0x2902).  The firmware adds one, which a client needs in order to subscribe: without
//  it CoreBluetooth reports "attribute not found" and never delivers the value (issue #496).
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

//
//  Meadow device name MeadowF7NotifyTest with company identifier Espressif (0x02e5).
//
//  This is the service NotifyCharacteristicSoakTest.py drives: an iteration count and a
//  client number the client writes, and a notifying field with the Client Characteristic
//  Configuration descriptor (0x2902) a client needs in order to subscribe.
//
const char *meadow_f7_notify_json = R"({
    "deviceName":"MeadowF7NotifyTest",
    "companyIdentifier":741,
    "services": [
        {
            "name":"NotifyService",
            "uuid":252,
            "characteristics": [
                {
                    "uuid":"018e99d6-8a61-11eb-8dcd-0242ac1300aa",
                    "permission":17,
                    "props":10,
                    "len":4
                },
                {
                    "uuid":"018e99d6-8a61-11eb-8dcd-0242ac1300bb",
                    "permission":17,
                    "props":10,
                    "len":4
                },
                {
                    "uuid":"018e99d6-8a61-11eb-8dcd-0242ac1300cc",
                    "permission":1,
                    "props":18,
                    "len":4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 17,
                        "len": 2,
                        "value": 0
                        }
                    ]
                }
            ]
        }
    ]
})";

//
//  One characteristic that takes both writes with and without a response (props 14) and
//  holds a value longer than one packet, for the write delivery test (issue #299).
//
const char *meadow_write_json = R"({
    "deviceName":"MeadowWriteTest",
    "companyIdentifier":741,
    "services": [
        {
            "name":"WriteService",
            "uuid":252,
            "characteristics": [
                {
                    "uuid":"01ae99d6-8a61-11eb-8dcd-0242ac1300dd",
                    "permission":17,
                    "props":14,
                    "len":512
                }
            ]
        }
    ]
})";

//
//  A notifying characteristic that declares no client configuration descriptor, which the
//  firmware adds, and a characteristic the client writes to say it has finished a step, for
//  the subscription test (issue #572).
//
const char *meadow_subscription_json = R"({
    "deviceName":"MeadowSubscribe",
    "companyIdentifier":741,
    "services": [
        {
            "name":"SubscriptionService",
            "uuid":251,
            "characteristics": [
                {
                    "uuid":"01be99d6-8a61-11eb-8dcd-0242ac1300ee",
                    "permission":17,
                    "props":18,
                    "len":4
                },
                {
                    "uuid":"01ce99d6-8a61-11eb-8dcd-0242ac1300ff",
                    "permission":17,
                    "props":8,
                    "len":4
                }
            ]
        }
    ]
})";

#endif // _TEST_BLUETOOTH_SERVICE_JSON_HPP_