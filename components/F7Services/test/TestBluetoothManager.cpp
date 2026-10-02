/*
 *  TestBluetoothManager.cpp
 *
 *  Tests for the parts of BluetoothManager and BluetoothDescriptor that need no radio:
 *  building the service graph from the JSON configuration and copying attribute values.
 */

#include "sdkconfig.h"

#include <string>
#include <string.h>
#include <type_traits>
#include "esp_gatt_defs.h"
#include "unity.h"
#include "esp_system.h"

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"
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
 * @brief Check that a configuration is refused and that nothing is left built behind it.
 *
 * @param manager Manager to build the graph in.
 * @param configuration JSON configuration that holds at least one unusable entry.
 * @param reason What is wrong with the configuration, for the failure message.
 */
static void CheckConfigurationIsRefused(BluetoothManager &manager, const std::string &configuration, const char *reason)
{
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(configuration.c_str()), reason);
    TEST_ASSERT_EQUAL_MESSAGE(0, manager.GetServiceDefinitions().size(), "A refused configuration left services behind.");
}

/**
 * @brief A characteristic of exactly the GATT maximum length is accepted.
 */
TEST_CASE("BuildGraphFromJson accepts a characteristic of the GATT maximum length", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::vector<BluetoothCharacteristic *> accepted = BuildSingleService(manager, ServiceJson(CharacteristicJson(0x2a01, ESP_GATT_MAX_ATTR_LEN, "")));

    TEST_ASSERT_EQUAL(1, accepted.size());
    TEST_ASSERT_EQUAL_UINT16(ESP_GATT_MAX_ATTR_LEN, accepted[0]->GetValue().MaxLength);

    manager.DeleteAllServices();
}

/**
 * @brief A characteristic one byte longer than the GATT maximum makes the configuration unusable.
 *
 * @note Since #297 a rejected entry refuses the whole configuration rather than being
 *       dropped from it, so the valid characteristic alongside it is not built either.
 */
TEST_CASE("BuildGraphFromJson refuses a characteristic longer than the GATT maximum", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string characteristics = CharacteristicJson(0x2a01, ESP_GATT_MAX_ATTR_LEN, "") + "," + CharacteristicJson(0x2a02, ESP_GATT_MAX_ATTR_LEN + 1, "");
    CheckConfigurationIsRefused(manager, ServiceJson(characteristics), "A characteristic longer than the GATT maximum was accepted.");
}

/**
 * @brief Lengths that are zero, negative or wrap to a small uint16_t each refuse the configuration.
 *
 * @note 70000 is 4464 once truncated to uint16_t, which fits the value storage but not the
 *       GATT response, so it has to be refused before the cast rather than after it.
 */
TEST_CASE("BuildGraphFromJson refuses a characteristic with a zero, negative or out of range length", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    for (const int length: {0, -1, 70000})
    {
        CheckConfigurationIsRefused(manager, ServiceJson(CharacteristicJson(0x2a01, length, "")), "A characteristic with an invalid length was accepted.");
    }
}

/**
 * @brief A descriptor longer than the GATT maximum, or with a zero length, refuses the configuration.
 */
TEST_CASE("BuildGraphFromJson refuses a descriptor longer than the GATT maximum", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    for (const int length: {ESP_GATT_MAX_ATTR_LEN + 1, 0})
    {
        const std::string descriptors = DescriptorJson(0x2902, 2) + "," + DescriptorJson(0x2901, length);
        CheckConfigurationIsRefused(manager, ServiceJson(CharacteristicJson(0x2a01, 4, descriptors)), "A descriptor with an invalid length was accepted.");
    }
}

/**
 * @brief A malformed service refuses the configuration wherever it appears in the array.
 *
 *  Regression check for issue #297.  AddServices used to return at the first malformed
 *  service, so a bad service first built nothing and a bad service later silently dropped
 *  every service after it, and in both cases Start reported success.
 */
TEST_CASE("BuildGraphFromJson refuses a configuration holding a malformed service", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string valid = "{\"name\":\"Good\",\"uuid\":254,\"characteristics\":[" + CharacteristicJson(0x2a01, 4, "") + "]}";
    const std::string noName = "{\"uuid\":253,\"characteristics\":[" + CharacteristicJson(0x2a02, 4, "") + "]}";
    const std::string textUuid = "{\"name\":\"Bad\",\"uuid\":\"fd\",\"characteristics\":[" + CharacteristicJson(0x2a03, 4, "") + "]}";

    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\",\"services\":[" + noName + "," + valid + "]}", "A malformed first service was accepted.");
    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\",\"services\":[" + valid + "," + textUuid + "]}", "A malformed later service was accepted.");
}

/**
 * @brief A configuration that parses but holds no usable service is refused.
 *
 *  Regression check for issue #297: an empty graph was reported as built, and Start went on
 *  to create a zero-length attribute table.
 */
TEST_CASE("BuildGraphFromJson refuses a configuration with no services", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\",\"services\":[]}", "An empty services array was accepted.");
    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\"}", "A configuration with no services key was accepted.");
    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\",\"services\":{}}", "A services key that is not an array was accepted.");
}

/**
 * @brief Longest device name the scan response holds whole: 31 bytes less the name's length and type bytes.
 */
#define LONGEST_ADVERTISED_NAME 29

/**
 * @brief Configuration holding one valid service and the given device name.
 *
 * @param deviceName Device name to configure.
 *
 * @return Configuration text for BluetoothManager::BuildGraphFromJson.
 */
static std::string NamedServiceJson(const std::string &deviceName)
{
    return ("{\"deviceName\":\"" + deviceName + "\",\"services\":[{\"name\":\"Test service\",\"uuid\":254,\"characteristics\":[" + CharacteristicJson(0x2a01, 4, "") + "]}]}");
}

/**
 * @brief A device name that fills the scan response is accepted and kept whole.
 */
TEST_CASE("BuildGraphFromJson accepts the longest device name the scan response holds", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const std::string deviceName(LONGEST_ADVERTISED_NAME, 'M');
    BuildSingleService(manager, NamedServiceJson(deviceName));
    TEST_ASSERT_EQUAL_STRING_MESSAGE(deviceName.c_str(), manager.GetDeviceName(), "The device name was not kept whole.");
}

/**
 * @brief A device name too long for the scan response refuses the configuration.
 *
 *  A longer name used to be accepted.  It was then advertised shortened or, past
 *  Bluedroid's own limit, refused by esp_ble_gap_set_device_name with nothing reported
 *  to the STM32 (issue #344).
 */
TEST_CASE("BuildGraphFromJson refuses a device name too long for the scan response", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    const size_t lengths[] = {LONGEST_ADVERTISED_NAME + 1, 64, 300};
    for (const size_t length: lengths)
    {
        CheckConfigurationIsRefused(manager, NamedServiceJson(std::string(length, 'M')), "A device name too long for the scan response was accepted.");
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(LONGEST_ADVERTISED_NAME, strlen(manager.GetDeviceName()), "A refused device name was kept.");
    }
}

/**
 * @brief A characteristics or descriptors key that is present but not an array refuses the configuration.
 */
TEST_CASE("BuildGraphFromJson refuses characteristics or descriptors that are not arrays", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    CheckConfigurationIsRefused(manager, "{\"deviceName\":\"MeadowTest\",\"services\":[{\"name\":\"S\",\"uuid\":254,\"characteristics\":{}}]}", "A characteristics object was accepted.");
    const std::string characteristic = "{\"uuid\":10755,\"permission\":17,\"props\":10,\"len\":4,\"descriptors\":7}";
    CheckConfigurationIsRefused(manager, ServiceJson(characteristic), "A descriptors number was accepted.");
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
    const uint16_t valueHandle = accepted[0]->GetValue().Handle;

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

/*
 *  Issue #449: the characteristic's declaration and value used to be public owning
 *  pointers, which the destructor deleted, so a caller could delete either one first and
 *  cause a double free.  They are now members reached through accessors returning
 *  references.  These checks are made at compile time, so the file does not build against
 *  a characteristic that still exposes them.
 */

/**
 * @brief True if Declaration can be reached from outside T.  A template, so that a member
 *        that is private or missing is a substitution failure, giving false, rather than an
 *        error.
 */
template<typename T> constexpr bool DECLARATION_IS_EXPOSED = requires(T & characteristic)
{
    characteristic.Declaration;
};

/**
 * @brief True if Value can be reached from outside T.
 */
template<typename T> constexpr bool VALUE_IS_EXPOSED = requires(T & characteristic)
{
    characteristic.Value;
};

static_assert(!DECLARATION_IS_EXPOSED<BluetoothCharacteristic>, "BluetoothCharacteristic exposes its declaration directly.");
static_assert(!VALUE_IS_EXPOSED<BluetoothCharacteristic>, "BluetoothCharacteristic exposes its value directly.");
static_assert(std::is_lvalue_reference_v<decltype(std::declval<BluetoothCharacteristic &>().GetDeclaration())>, "GetDeclaration must return a reference, not an owning pointer.");
static_assert(std::is_lvalue_reference_v<decltype(std::declval<BluetoothCharacteristic &>().GetValue())>, "GetValue must return a reference, not an owning pointer.");

/**
 * @brief Number of characteristics created and destroyed by the lifetime test.
 */
#define CHARACTERISTIC_LIFETIME_ITERATIONS 50

/**
 * @brief A characteristic's declaration and value are created with it, the declaration
 *        records the value's handle, and assigning a handle through the accessors reaches
 *        the characteristic's own objects.
 */
TEST_CASE("A characteristic owns its declaration and value", CLASS_OF_TESTS)
{
    BluetoothCharacteristic *characteristic = new BluetoothCharacteristic(Uuid16(0x2a19), CharacteristicPermission::Read, CharacteristicProperty::Read, 20);
    TEST_ASSERT_NOT_NULL(characteristic);

    TEST_ASSERT_EQUAL_UINT16(characteristic->GetValue().Handle, characteristic->GetDeclaration().ValueHandle);
    TEST_ASSERT_EQUAL_UINT16(20, characteristic->GetValue().MaxLength);

    characteristic->GetDeclaration().Handle = 0x0010;
    characteristic->GetValue().Handle = 0x0011;
    TEST_ASSERT_EQUAL_UINT16(0x0010, characteristic->GetDeclaration().Handle);
    TEST_ASSERT_EQUAL_UINT16(0x0011, characteristic->GetValue().Handle);
    TEST_ASSERT_EQUAL_PTR(&characteristic->GetValue(), &characteristic->GetValue());

    const BluetoothCharacteristic &readOnly = *characteristic;
    TEST_ASSERT_EQUAL_UINT16(0x0011, readOnly.GetValue().Handle);
    TEST_ASSERT_EQUAL_UINT16(0x0010, readOnly.GetDeclaration().Handle);

    delete characteristic;
}

/**
 * @brief Creating and destroying characteristics of both UUID sizes returns all of their
 *        memory, the declaration and value included, and frees nothing twice.
 *
 *  The test build uses comprehensive heap poisoning, so a double free would abort here
 *  rather than pass.
 */
TEST_CASE("Creating and destroying characteristics releases all of their memory", CLASS_OF_TESTS)
{
    //
    //  One round first, so anything allocated once on first use is not counted as a leak.
    //
    delete new BluetoothCharacteristic(Uuid16(0x2a19), CharacteristicPermission::Read, CharacteristicProperty::Read, 20);
    const uint32_t heapAtStart = esp_get_free_heap_size();

    for (uint32_t iteration = 0; iteration < CHARACTERISTIC_LIFETIME_ITERATIONS; iteration++)
    {
        delete new BluetoothCharacteristic(Uuid16(0x2a19), CharacteristicPermission::Read, CharacteristicProperty::Read | CharacteristicProperty::Notify, 64);
        delete new BluetoothCharacteristic(Uuid128("6e400001-b5a3-f393-e0a9-e50e24dcca9e"), CharacteristicPermission::Read | CharacteristicPermission::Write, CharacteristicProperty::Write, 128);
    }

    TEST_ASSERT_EQUAL_UINT32(heapAtStart, esp_get_free_heap_size());
}
