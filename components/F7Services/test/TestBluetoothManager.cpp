/*
 *  TestBluetoothManager.cpp
 *
 *  Tests for the parts of BluetoothManager and BluetoothDescriptor that need no radio:
 *  building the service graph from the JSON configuration and copying attribute values.
 */

#include "sdkconfig.h"

#include <string>
#include <string.h>
#include "esp_gatt_defs.h"
#include "unity.h"

#include "BluetoothAttribute.hpp"
#include "BluetoothDescriptor.hpp"
#include "BluetoothManager.hpp"
#include "Uuid.hpp"

#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief JSON for a descriptor with the given UUID and length.
 *
 * @param uuid 16-bit UUID for the descriptor.
 * @param length Value of the descriptor's "len" entry.
 *
 * @return JSON object text for the descriptor.
 */
static std::string DescriptorJson(const int uuid, const int length)
{
    return ("{\"uuid\":" + std::to_string(uuid) + ",\"permission\":17,\"len\":" + std::to_string(length) + ",\"value\":0}");
}

/**
 * @brief JSON for a characteristic with the given UUID, length and descriptors.
 *
 * @param uuid 16-bit UUID for the characteristic.
 * @param length Value of the characteristic's "len" entry.
 * @param descriptors Comma separated descriptor objects, or an empty string for none.
 *
 * @return JSON object text for the characteristic.
 */
static std::string CharacteristicJson(const int uuid, const int length, const std::string &descriptors)
{
    std::string json = "{\"uuid\":" + std::to_string(uuid) + ",\"permission\":17,\"props\":10,\"len\":" + std::to_string(length);
    if (!descriptors.empty())
    {
        json += ",\"descriptors\":[" + descriptors + "]";
    }
    return (json + "}");
}

/**
 * @brief JSON configuration holding a single service with the given characteristics.
 *
 * @param characteristics Comma separated characteristic objects.
 *
 * @return Configuration text for BluetoothManager::BuildGraphFromJson.
 */
static std::string ServiceJson(const std::string &characteristics)
{
    return ("{\"deviceName\":\"MeadowTest\",\"services\":[{\"name\":\"Test service\",\"uuid\":254,\"characteristics\":[" + characteristics + "]}]}");
}

/**
 * @brief Build the graph from the configuration and return the characteristics of its only service.
 *
 * @param manager Manager to build the graph in.
 * @param configuration JSON configuration holding exactly one service.
 *
 * @return Characteristics the manager accepted.
 */
static std::vector<BluetoothCharacteristic *> BuildSingleService(BluetoothManager &manager, const std::string &configuration)
{
    TEST_ASSERT_TRUE_MESSAGE(manager.BuildGraphFromJson(configuration.c_str()), "The configuration was not parsed.");
    const std::vector<BluetoothService *> services = manager.GetServiceDefinitions();
    TEST_ASSERT_EQUAL_MESSAGE(1, services.size(), "Expected exactly one service.");
    return (services[0]->GetCharacteristicDefinitions());
}

/**
 * @brief A characteristic of exactly the GATT maximum length is kept and one byte longer is ignored.
 */
TEST_CASE("BuildGraphFromJson ignores a characteristic longer than the GATT maximum", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string characteristics = CharacteristicJson(0x2a01, ESP_GATT_MAX_ATTR_LEN, "") + "," + CharacteristicJson(0x2a02, ESP_GATT_MAX_ATTR_LEN + 1, "");

    const std::vector<BluetoothCharacteristic *> accepted = BuildSingleService(manager, ServiceJson(characteristics));

    TEST_ASSERT_EQUAL_MESSAGE(1, accepted.size(), "Only the characteristic within the GATT maximum should be kept.");
    TEST_ASSERT_EQUAL_UINT16(ESP_GATT_MAX_ATTR_LEN, accepted[0]->Value->MaxLength);

    manager.DeleteAllServices();
}

/**
 * @brief Lengths that are zero, negative or wrap to a small uint16_t are all refused.
 *
 * @note 70000 is 4464 once truncated to uint16_t, which fits the value storage but not the
 *       GATT response, so it has to be refused before the cast rather than after it.
 */
TEST_CASE("BuildGraphFromJson ignores a characteristic with a zero, negative or out of range length", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string characteristics = CharacteristicJson(0x2a01, 0, "") + "," + CharacteristicJson(0x2a02, -1, "") + "," + CharacteristicJson(0x2a03, 70000, "");

    const std::vector<BluetoothCharacteristic *> accepted = BuildSingleService(manager, ServiceJson(characteristics));

    TEST_ASSERT_EQUAL_MESSAGE(0, accepted.size(), "No characteristic with an invalid length should be kept.");

    manager.DeleteAllServices();
}

/**
 * @brief A descriptor longer than the GATT maximum, or with a zero length, is ignored and the
 *        valid descriptor alongside it is kept.
 */
TEST_CASE("BuildGraphFromJson ignores a descriptor longer than the GATT maximum", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string descriptors = DescriptorJson(0x2901, ESP_GATT_MAX_ATTR_LEN + 1) + "," + DescriptorJson(0x2904, 0) + "," + DescriptorJson(0x2902, 2);

    const std::vector<BluetoothCharacteristic *> accepted = BuildSingleService(manager, ServiceJson(CharacteristicJson(0x2a01, 4, descriptors)));

    TEST_ASSERT_EQUAL(1, accepted.size());
    const std::vector<BluetoothDescriptor *> keptDescriptors = accepted[0]->GetDescriptorDefinitions();
    TEST_ASSERT_EQUAL_MESSAGE(1, keptDescriptors.size(), "Only the descriptor with a valid length should be kept.");
    TEST_ASSERT_EQUAL_UINT16(2, keptDescriptors[0]->MaxLength);

    manager.DeleteAllServices();
}

/**
 * @brief Reading a value never copies more than the capacity of the caller's buffer.
 *
 * @note The GATT read handler passes a fixed ESP_GATT_MAX_ATTR_LEN byte buffer on the
 *       Bluedroid task's stack; a capacity smaller than the value stands in for it here so
 *       any overrun lands in the guard bytes rather than on the stack.
 */
TEST_CASE("GetCurrentValueForAttributeHandle copies no more than the capacity", CLASS_OF_TESTS)
{
    static constexpr uint16_t VALUE_LENGTH = 32;
    static constexpr uint16_t SHORT_CAPACITY = 8;
    static constexpr uint8_t GUARD = 0x55;

    BluetoothManager manager;
    const std::vector<BluetoothCharacteristic *> accepted = BuildSingleService(manager, ServiceJson(CharacteristicJson(0x2a01, VALUE_LENGTH, "")));
    TEST_ASSERT_EQUAL(1, accepted.size());

    //
    //  Service, characteristic declaration, characteristic value.
    //
    const uint16_t handles[] = {0x0028, 0x0029, 0x002a};
    TEST_ASSERT_TRUE(manager.SetHandles(handles, sizeof(handles) / sizeof(handles[0])));
    const uint16_t valueHandle = accepted[0]->Value->Handle;

    uint8_t pattern[VALUE_LENGTH];
    for (uint16_t index = 0; index < VALUE_LENGTH; index++)
    {
        pattern[index] = static_cast<uint8_t>(index + 1);
    }
    manager.SetCurrentValueForAttributeHandle(valueHandle, pattern, VALUE_LENGTH);

    uint8_t buffer[VALUE_LENGTH + 8];
    memset(buffer, GUARD, sizeof(buffer));
    uint16_t length = 0;
    TEST_ASSERT_TRUE(manager.GetCurrentValueForAttributeHandle(valueHandle, buffer, SHORT_CAPACITY, &length));
    TEST_ASSERT_EQUAL_UINT16(SHORT_CAPACITY, length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pattern, buffer, SHORT_CAPACITY);
    for (uint16_t index = SHORT_CAPACITY; index < sizeof(buffer); index++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(GUARD, buffer[index], "A byte beyond the capacity was written.");
    }

    memset(buffer, GUARD, sizeof(buffer));
    TEST_ASSERT_TRUE(manager.GetCurrentValueForAttributeHandle(valueHandle, buffer, sizeof(buffer), &length));
    TEST_ASSERT_EQUAL_UINT16(VALUE_LENGTH, length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pattern, buffer, VALUE_LENGTH);
    TEST_ASSERT_EQUAL_HEX8(GUARD, buffer[VALUE_LENGTH]);

    manager.DeleteAllServices();
}

/**
 * @brief The descriptor constructor reads only valueLength bytes from the source and pads the rest.
 */
TEST_CASE("BluetoothDescriptor copies no more than the value length it is given", CLASS_OF_TESTS)
{
    static constexpr uint16_t MAXIMUM_LENGTH = 8;
    const uint8_t shortValue[3] = {0x11, 0x22, 0x33};

    BluetoothDescriptor shortDescriptor(nullptr, Uuid16(static_cast<uint16_t>(0x2901)), CharacteristicPermission::Read, MAXIMUM_LENGTH, shortValue, sizeof(shortValue));
    TEST_ASSERT_EQUAL_UINT16(sizeof(shortValue), shortDescriptor.CurrentLength);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(shortValue, shortDescriptor.GetValue(), sizeof(shortValue));
    for (uint16_t index = sizeof(shortValue); index < MAXIMUM_LENGTH; index++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0, shortDescriptor.GetValue()[index], "The unused part of the value was not zeroed.");
    }

    uint8_t longValue[MAXIMUM_LENGTH * 2];
    memset(longValue, 0xaa, sizeof(longValue));
    BluetoothDescriptor longDescriptor(nullptr, Uuid16(static_cast<uint16_t>(0x2901)), CharacteristicPermission::Read, MAXIMUM_LENGTH, longValue, sizeof(longValue));
    TEST_ASSERT_EQUAL_UINT16(MAXIMUM_LENGTH, longDescriptor.CurrentLength);

    BluetoothDescriptor emptyDescriptor(nullptr, Uuid16(static_cast<uint16_t>(0x2901)), CharacteristicPermission::Read, MAXIMUM_LENGTH, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT16(0, emptyDescriptor.CurrentLength);
}
