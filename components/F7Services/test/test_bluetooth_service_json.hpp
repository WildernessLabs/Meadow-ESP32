/**
 * @file test_bluetooth_service_json.hpp
 * @author Mark Stevens
 *
 * @brief JSON encoding of the Bluetooth service configuration.
 *
 * @copyright Copyright (c) 2025
 */
#ifndef __TEST_BLUETOOTH_SERVICE_JSON_HPP__
#define __TEST_BLUETOOTH_SERVICE_JSON_HPP__

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
//  The following three fragments of a service definition are designed to allow the dynamic
//  creation of a service defintion for a Meadow F7 Bluetooth service.
//
//  The first fragment defines the basic device properties.  The next properties are missing
//  here and can be dynamically added to the JSON.  The missing fragments are:
//      - passKey
//      - ioCapabilities
//      - authorisationRequirements
//
const char *meadow_f7_service_header = R"({
    "deviceName":"MeadowF7",
    "companyIdentifier":741,
)";

//
//  The next section will contain the characteristic definition.  The first part of the
//  definition is as follows and should be followed by the permissions property.
//
const char *characteristic_start = R"(    "services": [
        {
            "name":"GattService",
            "uuid":253,
            "characteristics": [
                {
                    "uuid":"019e99d6-8a61-11eb-8dcd-0242ac1300cc",
)";

//
//  The final part of the characteristic definition is as follows:
//
const char *characteristic_end = R"(
                    "props":10,
                    "len":4
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

#endif // __TEST_BLUETOOTH_SERVICE_JSON_HPP__