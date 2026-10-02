/*
 *  TestBluetoothAdvertising.cpp
 *
 *  Tests that the advertising packet and the scan response fit in 31 bytes (issue #345).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include <string>
#include "unity.h"

#include "BluetoothRequestHandler.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief 16-bit UUID of the service in the test configuration.
 */
#define ADVERTISING_TEST_SERVICE_UUID 254

/**
 * @brief Length of a 16-bit UUID in an advertising structure.
 */
#define UUID_16_LENGTH 2

/**
 * @brief Length of a 32-bit UUID in an advertising structure.
 */
#define UUID_32_LENGTH 4

/**
 * @brief Length of the length and type bytes that start every advertising structure.
 */
#define AD_STRUCTURE_HEADER_LENGTH 2

/**
 * @brief Gives the tests access to BluetoothRequestHandler's private advertising data.
 */
class TestBluetoothAdvertising
{
public:
    /**
     * @brief Largest advertising packet or scan response, as the handler defines it.
     */
    static constexpr uint32_t MAXIMUM_LENGTH = BluetoothRequestHandler::MAXIMUM_ADVERTISING_DATA_LENGTH;

    /**
     * @brief Configure the Bluetooth manager, then generate the advertising data on a handler of the test's own.
     *
     *  The handler is created without Setup(), so it starts no task and no Bluetooth stack.
     *  The test dispatcher may already hold the application's handler, which lives for the
     *  whole run, so that one is set aside here and put back by Release().  Start rebuilds
     *  the manager's graph from its own configuration, so replacing it here is harmless.
     *
     * @return The handler holding the generated advertising data.
     */
    static BluetoothRequestHandler *Generate()
    {
        const std::string configuration = "{\"deviceName\":\"MeadowF7\",\"companyIdentifier\":741,\"services\":[{\"name\":\"Service A\",\"uuid\":" + std::to_string(ADVERTISING_TEST_SERVICE_UUID) + ",\"characteristics\":[{\"uuid\":10755,\"permission\":17,\"props\":10,\"len\":4}]}]}";
        TEST_ASSERT_TRUE_MESSAGE(BluetoothRequestHandler::_manager.BuildGraphFromJson(configuration.c_str()), "The configuration was not parsed.");

        //
        //  The dispatcher is a static shared by every handler, so the handler made here
        //  replaces it for all of them until Release() puts it back.
        //
        _savedInstance = BluetoothRequestHandler::_instance;
        _savedInstantiated = BluetoothRequestHandler::_instantiated;
        _savedDispatcher = BluetoothRequestHandler::_messageDispatcher;
        BluetoothRequestHandler::_instance = nullptr;
        BluetoothRequestHandler::_instantiated = false;
        BluetoothRequestHandler *handler = new BluetoothRequestHandler(nullptr);
        TEST_ASSERT_NOT_NULL(handler);
        handler->GenerateAdvertisingData();
        return (handler);
    }

    /**
     * @brief Delete a handler made by Generate() and put back the handler it set aside.
     *
     * @param handler Handler to delete.
     */
    static void Release(BluetoothRequestHandler *handler)
    {
        delete handler;
        BluetoothRequestHandler::_instance = _savedInstance;
        BluetoothRequestHandler::_instantiated = _savedInstantiated;
        BluetoothRequestHandler::_messageDispatcher = _savedDispatcher;
        _savedInstance = nullptr;
        _savedDispatcher = nullptr;
    }

    /**
     * @brief The advertising packet the handler generated.
     *
     * @param handler Handler holding the data.
     *
     * @return The advertising packet's description.
     */
    static const esp_ble_adv_data_t &AdvertisingData(BluetoothRequestHandler *handler)
    {
        return (handler->_advertisingData);
    }

    /**
     * @brief The scan response the handler generated.
     *
     * @param handler Handler holding the data.
     *
     * @return The scan response's description.
     */
    static const esp_ble_adv_data_t &ScanResponseData(BluetoothRequestHandler *handler)
    {
        return (handler->_scanResponseData);
    }

private:
    /**
     * @brief Handler set aside while the test's own is in use.
     */
    static BluetoothRequestHandler *_savedInstance;

    /**
     * @brief Instantiation guard set aside while the test's own handler is in use.
     */
    static bool _savedInstantiated;

    /**
     * @brief Shared dispatcher set aside while the test's own handler is in use.
     */
    static IMessageDispatcher *_savedDispatcher;
};

BluetoothRequestHandler *TestBluetoothAdvertising::_savedInstance = nullptr;
bool TestBluetoothAdvertising::_savedInstantiated = false;
IMessageDispatcher *TestBluetoothAdvertising::_savedDispatcher = nullptr;

/**
 * @brief Bytes the service UUID list takes once Bluedroid has shortened the UUIDs.
 *
 *  Bluedroid takes each UUID as 16 bytes.  One built on the Bluetooth base UUID goes out as
 *  16 bits, or 32 if bytes 14 and 15 are set; any other goes out whole.  Each size that is
 *  present is a structure of its own.
 *
 * @param uuids Packed 128-bit UUIDs.
 * @param length Length of uuids in bytes.
 *
 * @return Encoded length, headers included.
 */
static uint32_t EncodedServiceUuidLength(const uint8_t *uuids, const uint16_t length)
{
    static const uint8_t BASE_PREFIX[12] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
    uint32_t uuid16 = 0;
    uint32_t uuid32 = 0;
    uint32_t uuid128 = 0;
    for (uint16_t offset = 0; (offset + ESP_UUID_LEN_128) <= length; offset += ESP_UUID_LEN_128)
    {
        const uint8_t *uuid = uuids + offset;
        if (memcmp(uuid, BASE_PREFIX, sizeof(BASE_PREFIX)) == 0)
        {
            if ((uuid[14] == 0) && (uuid[15] == 0))
            {
                uuid16++;
            }
            else
            {
                uuid32++;
            }
        }
        else
        {
            uuid128++;
        }
    }

    uint32_t result = 0;
    result += (uuid16 == 0) ? 0 : AD_STRUCTURE_HEADER_LENGTH + (uuid16 * UUID_16_LENGTH);
    result += (uuid32 == 0) ? 0 : AD_STRUCTURE_HEADER_LENGTH + (uuid32 * UUID_32_LENGTH);
    result += (uuid128 == 0) ? 0 : AD_STRUCTURE_HEADER_LENGTH + (uuid128 * ESP_UUID_LEN_128);
    return (result);
}

/**
 * @brief Bytes the described data takes on air, following Bluedroid's encoding.
 *
 * @param data Advertising packet or scan response description.
 * @param nameLength Length of the device name.
 *
 * @return Encoded length.
 */
static uint32_t EncodedLength(const esp_ble_adv_data_t &data, const size_t nameLength)
{
    uint32_t result = 0;
    result += (data.flag == 0) ? 0 : 3;
    result += data.include_name ? (AD_STRUCTURE_HEADER_LENGTH + nameLength) : 0;
    result += data.include_txpower ? 3 : 0;
    result += ((data.min_interval == 0) && (data.max_interval == 0)) ? 0 : 6;
    result += (data.appearance == 0) ? 0 : 4;
    result += (data.manufacturer_len == 0) ? 0 : (AD_STRUCTURE_HEADER_LENGTH + data.manufacturer_len);
    result += (data.service_data_len == 0) ? 0 : (AD_STRUCTURE_HEADER_LENGTH + data.service_data_len);
    result += (data.p_service_uuid == nullptr) ? 0 : EncodedServiceUuidLength(data.p_service_uuid, data.service_uuid_len);
    return (result);
}

TEST_CASE("Advertising packet and scan response fit in 31 bytes with the longest name the scan response holds", CLASS_OF_TESTS)
{
    //
    //  The scan response holds the name and nothing else, so its longest name is 31 bytes
    //  less the structure header.
    //
    const std::string deviceName(TestBluetoothAdvertising::MAXIMUM_LENGTH - AD_STRUCTURE_HEADER_LENGTH, 'M');
    BluetoothRequestHandler *handler = TestBluetoothAdvertising::Generate();
    //
    //  The descriptions point into the handler, so they are measured before it is released.
    //
    const esp_ble_adv_data_t &advertisingData = TestBluetoothAdvertising::AdvertisingData(handler);
    const esp_ble_adv_data_t &scanResponseData = TestBluetoothAdvertising::ScanResponseData(handler);
    const bool advertisingIsScanResponse = advertisingData.set_scan_rsp;
    const bool scanResponseIsScanResponse = scanResponseData.set_scan_rsp;
    const bool scanResponseHasName = scanResponseData.include_name;
    const uint32_t advertisingLength = EncodedLength(advertisingData, deviceName.length());
    const uint32_t scanResponseLength = EncodedLength(scanResponseData, deviceName.length());
    TestBluetoothAdvertising::Release(handler);

    TEST_ASSERT_FALSE_MESSAGE(advertisingIsScanResponse, "The advertising packet is marked as a scan response.");
    TEST_ASSERT_TRUE_MESSAGE(scanResponseIsScanResponse, "The scan response is not marked as one.");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(TestBluetoothAdvertising::MAXIMUM_LENGTH, advertisingLength, "The advertising packet is longer than 31 bytes.");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(TestBluetoothAdvertising::MAXIMUM_LENGTH, scanResponseLength, "The scan response is longer than 31 bytes.");
    TEST_ASSERT_TRUE_MESSAGE(scanResponseHasName, "The scan response does not carry the name.");
}

TEST_CASE("Advertising packet carries the configured service UUID", CLASS_OF_TESTS)
{
    BluetoothRequestHandler *handler = TestBluetoothAdvertising::Generate();
    const esp_ble_adv_data_t &advertisingData = TestBluetoothAdvertising::AdvertisingData(handler);
    const uint16_t uuidLength = advertisingData.service_uuid_len;
    uint8_t uuid[ESP_UUID_LEN_128];
    memset(uuid, 0, sizeof(uuid));
    if ((advertisingData.p_service_uuid != nullptr) && (advertisingData.service_uuid_len == sizeof(uuid)))
    {
        memcpy(uuid, advertisingData.p_service_uuid, sizeof(uuid));
    }
    TestBluetoothAdvertising::Release(handler);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(ESP_UUID_LEN_128, uuidLength, "The advertising packet does not carry exactly one service UUID.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ADVERTISING_TEST_SERVICE_UUID & 0xff, uuid[12], "The advertised service UUID is not the configured one.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((ADVERTISING_TEST_SERVICE_UUID >> 8) & 0xff, uuid[13], "The advertised service UUID is not the configured one.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, uuid[14], "The advertised service UUID is not a 16-bit UUID.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, uuid[15], "The advertised service UUID is not a 16-bit UUID.");
}
