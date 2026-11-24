/*
 *  TestBluetoothSecurity.cpp
 *
 *  Tests for the Bluetooth security settings (issue #398): reading them from the configuration,
 *  what they ask of the stack, and how they protect the attributes in the service graph.  None
 *  of this needs a radio.
 */

#include "sdkconfig.h"

#include <cstdio>
#include <string>
#include <utility>
#include "esp_gap_ble_api.h"
#include "unity.h"

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothDescriptor.hpp"
#include "BluetoothManager.hpp"
#include "BluetoothSecurityConfiguration.hpp"

#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief Permission bits, as the configuration gives them.
 */
#define PERMISSION_READ 1U
#define PERMISSION_READ_ENCRYPTED 2U
#define PERMISSION_READ_MITM 4U
#define PERMISSION_WRITE 16U
#define PERMISSION_WRITE_ENCRYPTED 32U
#define PERMISSION_WRITE_MITM 64U

/**
 * @brief Property bits, as the configuration gives them.
 */
#define PROPERTY_READ 2U
#define PROPERTY_WRITE 8U
#define PROPERTY_NOTIFY 16U

/**
 * @brief 16-bit UUID of the Characteristic User Description descriptor.
 */
#define USER_DESCRIPTION_UUID 0x2901

/**
 * @brief JSON configuration with one service holding one characteristic.
 *
 * @param security The "security" object, or an empty string for a configuration without one.
 * @param permission Permission of the characteristic.
 * @param properties Properties of the characteristic.
 * @param descriptors Comma separated descriptor objects, or an empty string for none.
 *
 * @return Configuration text for BluetoothManager::BuildGraphFromJson.
 */
static std::string Configuration(const std::string &security, const int permission = PERMISSION_READ | PERMISSION_WRITE, const int properties = PROPERTY_READ | PROPERTY_WRITE, const std::string &descriptors = "")
{
    std::string json = R"({"deviceName":"MeadowSecure",)";
    if (!security.empty())
    {
        json += R"("security":)" + security + ',';
    }
    json += R"("services":[{"name":"Secure service","uuid":254,"characteristics":[{"uuid":10755,"permission":)" + std::to_string(permission) + R"(,"props":)" + std::to_string(properties) + R"(,"len":4)";
    if (!descriptors.empty())
    {
        json += R"(,"descriptors":[)" + descriptors + ']';
    }
    return (json + "}]}]}");
}

/**
 * @brief JSON for a descriptor.
 *
 * @param uuid 16-bit UUID of the descriptor.
 * @param permission Permission of the descriptor.
 * @param length Length of the descriptor.
 *
 * @return JSON object text for the descriptor.
 */
static std::string Descriptor(const int uuid, const int permission, const int length)
{
    return (R"({"uuid":)" + std::to_string(uuid) + R"(,"permission":)" + std::to_string(permission) + R"(,"len":)" + std::to_string(length) + R"(,"value":0})");
}

/**
 * @brief Build the graph from a configuration that must be accepted and return its only characteristic.
 *
 * @param manager Manager to build the graph in.
 * @param configuration Configuration holding one service with one characteristic.
 *
 * @return The characteristic.
 */
static BluetoothCharacteristic *BuildOne(BluetoothManager &manager, const std::string &configuration)
{
    TEST_ASSERT_TRUE_MESSAGE(manager.BuildGraphFromJson(configuration.c_str()), "The configuration was refused.");
    const std::vector<BluetoothService *> services = manager.GetServiceDefinitions();
    TEST_ASSERT_EQUAL(1, services.size());
    TEST_ASSERT_EQUAL(1, services[0]->GetCharacteristicDefinitions().size());
    return (services[0]->GetCharacteristicDefinitions()[0]);
}

/**
 * @brief Find a 16-bit descriptor of a characteristic.
 *
 * @param characteristic Characteristic to look in.
 * @param uuid UUID of the descriptor.
 *
 * @return The descriptor, nullptr if there is none.
 */
static BluetoothDescriptor *FindDescriptor(BluetoothCharacteristic *characteristic, const uint16_t uuid)
{
    for (BluetoothDescriptor *descriptor: characteristic->GetDescriptorDefinitions())
    {
        if (!descriptor->Is128() && (static_cast<uint16_t>(descriptor->uuid16) == uuid))
        {
            return (descriptor);
        }
    }
    return (nullptr);
}

/**
 * @brief Check that settings hold the defaults.
 *
 * @param security Settings to check.
 */
static void CheckDefaults(const BluetoothSecurityConfiguration &security)
{
    TEST_ASSERT_EQUAL_MESSAGE(std::to_underlying(BluetoothSecurityConfiguration::IoCapability::NoInputNoOutput), std::to_underlying(security.Capability), "The default capability is not NoInputNoOutput.");
    TEST_ASSERT_TRUE_MESSAGE(security.Bonding, "Bonding is not on by default.");
    TEST_ASSERT_FALSE_MESSAGE(security.Mitm, "MITM protection is on by default.");
    TEST_ASSERT_FALSE_MESSAGE(security.SecureConnectionsOnly, "Secure Connections only is on by default.");
    TEST_ASSERT_FALSE_MESSAGE(security.EncryptOnConnect, "Encryption on connection is on by default.");
    TEST_ASSERT_TRUE_MESSAGE(security.AcceptPairing, "Pairing is not accepted by default.");
}

/**
 * @brief A configuration with no security settings behaves as the service always has.
 *
 *  Just Works pairing with bonding, whatever the client negotiates, and no encryption unless the
 *  client asks.  An STM32 application that knows nothing of pairing must keep working.
 */
TEST_CASE("A configuration without security settings gets the defaults", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    BuildOne(manager, Configuration(""));

    const BluetoothSecurityConfiguration security = manager.GetSecurityConfiguration();
    CheckDefaults(security);
    TEST_ASSERT_EQUAL_HEX8(ESP_LE_AUTH_REQ_SC_ONLY | ESP_LE_AUTH_BOND, security.AuthenticationRequirement());
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE, security.OnlyAcceptSpecifiedAuthentication(), "The default settings refuse pairing a client used to be able to do.");
    TEST_ASSERT_EQUAL(ESP_BLE_SEC_ENCRYPT_NO_MITM, security.EncryptionLevel());

    manager.DeleteAllServices();
}

TEST_CASE("Every security setting is read from the configuration", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    BuildOne(manager, Configuration(R"({"ioCapability":1,"bonding":false,"mitm":true,"secureConnectionsOnly":true,"encryptOnConnect":true,"acceptPairing":false})"));

    const BluetoothSecurityConfiguration security = manager.GetSecurityConfiguration();
    TEST_ASSERT_EQUAL(std::to_underlying(BluetoothSecurityConfiguration::IoCapability::DisplayYesNo), std::to_underlying(security.Capability));
    TEST_ASSERT_FALSE(security.Bonding);
    TEST_ASSERT_TRUE(security.Mitm);
    TEST_ASSERT_TRUE(security.SecureConnectionsOnly);
    TEST_ASSERT_TRUE(security.EncryptOnConnect);
    TEST_ASSERT_FALSE(security.AcceptPairing);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(ESP_LE_AUTH_REQ_SC_ONLY | ESP_LE_AUTH_REQ_MITM, security.AuthenticationRequirement(), "Without bonding the bond bit must not be asked for.");
    TEST_ASSERT_EQUAL(ESP_BLE_SEC_ENCRYPT_MITM, security.EncryptionLevel());

    manager.DeleteAllServices();
}

TEST_CASE("Each input and output capability is accepted", CLASS_OF_TESTS)
{
    const BluetoothSecurityConfiguration::IoCapability capabilities[] = {
        BluetoothSecurityConfiguration::IoCapability::DisplayOnly,     BluetoothSecurityConfiguration::IoCapability::DisplayYesNo,    BluetoothSecurityConfiguration::IoCapability::KeyboardOnly,
        BluetoothSecurityConfiguration::IoCapability::NoInputNoOutput, BluetoothSecurityConfiguration::IoCapability::KeyboardDisplay,
    };
    for (const BluetoothSecurityConfiguration::IoCapability capability: capabilities)
    {
        BluetoothManager manager;
        BuildOne(manager, Configuration(R"({"ioCapability":)" + std::to_string(std::to_underlying(capability)) + '}'));
        TEST_ASSERT_EQUAL(std::to_underlying(capability), std::to_underlying(manager.GetSecurityConfiguration().Capability));
        manager.DeleteAllServices();
    }
}

/**
 * @brief A security setting that cannot be used refuses the whole configuration, as any other
 *        unusable entry does, and leaves the defaults behind.
 */
TEST_CASE("A malformed security setting refuses the configuration", CLASS_OF_TESTS)
{
    const char *malformed[] = {
        "[]", "1", "null", R"({"ioCapability":5})", R"({"ioCapability":-1})", R"({"ioCapability":1.5})", R"({"ioCapability":"1"})", R"({"bonding":1})", R"({"mitm":"true"})", R"({"secureConnectionsOnly":null})", R"({"encryptOnConnect":0})", R"({"acceptPairing":"no"})",
    };
    for (const char *security: malformed)
    {
        BluetoothManager manager;
        //
        //  Settings that differ from the defaults are accepted first, so the refusal can be seen
        //  to put the defaults back rather than keep the last settings.
        //
        BuildOne(manager, Configuration(R"({"ioCapability":4,"acceptPairing":false})"));
        manager.DeleteAllServices();

        printf("Security settings: %s\n", security);
        TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration(security).c_str()), "A malformed security setting was accepted.");
        TEST_ASSERT_EQUAL_MESSAGE(0, manager.GetServiceDefinitions().size(), "A refused configuration left services behind.");
        CheckDefaults(manager.GetSecurityConfiguration());
    }
}

/**
 * @brief MITM protection needs input or output to authenticate with.
 *
 *  The example settings asked for MITM protection with no input or output, which the stack
 *  quietly turned into Just Works (issue #398, point 1).
 */
TEST_CASE("MITM protection without input or output refuses the configuration", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration(R"({"mitm":true})").c_str()), "MITM protection was accepted with the default capability, NoInputNoOutput.");
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration(R"({"ioCapability":3,"mitm":true})").c_str()), "MITM protection was accepted with NoInputNoOutput.");

    BuildOne(manager, Configuration(R"({"ioCapability":0,"mitm":true})"));
    TEST_ASSERT_TRUE(manager.GetSecurityConfiguration().Mitm);
    manager.DeleteAllServices();
}

/**
 * @brief The stack is told to refuse a pairing weaker than the one asked for.
 *
 *  With ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE the stack accepts whatever the client
 *  negotiates (issue #398, point 3), so MITM protection and Secure Connections only each switch
 *  it on.
 */
TEST_CASE("MITM protection or Secure Connections only makes the stack refuse weaker pairing", CLASS_OF_TESTS)
{
    BluetoothSecurityConfiguration security;
    TEST_ASSERT_EQUAL_HEX8(ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE, security.OnlyAcceptSpecifiedAuthentication());

    security.SecureConnectionsOnly = true;
    TEST_ASSERT_EQUAL_HEX8(ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE, security.OnlyAcceptSpecifiedAuthentication());
    TEST_ASSERT_EQUAL_HEX8(ESP_LE_AUTH_REQ_SC_ONLY | ESP_LE_AUTH_BOND, security.AuthenticationRequirement());

    security.SecureConnectionsOnly = false;
    security.Capability = BluetoothSecurityConfiguration::IoCapability::KeyboardDisplay;
    security.Mitm = true;
    TEST_ASSERT_EQUAL_HEX8(ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE, security.OnlyAcceptSpecifiedAuthentication());
    TEST_ASSERT_EQUAL_HEX8(ESP_LE_AUTH_REQ_SC_MITM_BOND, security.AuthenticationRequirement());
}

/**
 * @brief An attribute that needs MITM protection is refused unless the settings demand it.
 *
 *  Otherwise no pairing could ever reach it, which the client would find out only by failing.
 *  An attribute that needs encryption alone is reachable with Just Works, so it is accepted.
 */
TEST_CASE("An attribute that needs MITM protection needs settings that demand it", CLASS_OF_TESTS)
{
    BluetoothManager manager;
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration("", PERMISSION_READ_MITM | PERMISSION_WRITE).c_str()), "A characteristic read only with MITM protection was accepted without it.");
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration(R"({"ioCapability":1})", PERMISSION_READ | PERMISSION_WRITE_MITM).c_str()), "A characteristic written only with MITM protection was accepted without it.");
    TEST_ASSERT_FALSE_MESSAGE(manager.BuildGraphFromJson(Configuration("", PERMISSION_READ | PERMISSION_WRITE, PROPERTY_READ | PROPERTY_WRITE, Descriptor(USER_DESCRIPTION_UUID, PERMISSION_READ_MITM, 8)).c_str()), "A descriptor read only with MITM protection was accepted without it.");
    TEST_ASSERT_EQUAL(0, manager.GetServiceDefinitions().size());

    BuildOne(manager, Configuration(R"({"ioCapability":1,"mitm":true})", PERMISSION_READ_MITM | PERMISSION_WRITE_MITM));
    manager.DeleteAllServices();

    BuildOne(manager, Configuration("", PERMISSION_READ_ENCRYPTED | PERMISSION_WRITE_ENCRYPTED));
    manager.DeleteAllServices();
}

/**
 * @brief Subscribing to a protected characteristic needs the same security as reading it.
 *
 *  The stack sends a notification to a subscribed client without checking the value's own
 *  permission, so the protection has to be on the client configuration descriptor.  Otherwise a
 *  client that has not paired could subscribe and be sent the value.
 */
TEST_CASE("Subscribing to a protected characteristic needs the security its value does", CLASS_OF_TESTS)
{
    const auto plain = std::to_underlying(CharacteristicPermission::Read | CharacteristicPermission::Write);
    BluetoothManager manager;

    BluetoothCharacteristic *characteristic = BuildOne(manager, Configuration("", PERMISSION_READ | PERMISSION_WRITE, PROPERTY_READ | PROPERTY_NOTIFY));
    BluetoothDescriptor *configuration = FindDescriptor(characteristic, CLIENT_CHARACTERISTIC_CONFIGURATION_UUID);
    TEST_ASSERT_NOT_NULL(configuration);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(plain, std::to_underlying(configuration->Permission), "An unprotected characteristic's subscription was changed.");
    manager.DeleteAllServices();

    characteristic = BuildOne(manager, Configuration("", PERMISSION_READ_ENCRYPTED | PERMISSION_WRITE, PROPERTY_READ | PROPERTY_NOTIFY));
    configuration = FindDescriptor(characteristic, CLIENT_CHARACTERISTIC_CONFIGURATION_UUID);
    TEST_ASSERT_NOT_NULL(configuration);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(plain | PERMISSION_WRITE_ENCRYPTED, std::to_underlying(configuration->Permission), "Subscribing to an encrypted characteristic does not need encryption.");
    manager.DeleteAllServices();

    characteristic = BuildOne(manager, Configuration(R"({"ioCapability":4,"mitm":true})", PERMISSION_READ_MITM | PERMISSION_WRITE, PROPERTY_READ | PROPERTY_NOTIFY));
    configuration = FindDescriptor(characteristic, CLIENT_CHARACTERISTIC_CONFIGURATION_UUID);
    TEST_ASSERT_NOT_NULL(configuration);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(plain | PERMISSION_WRITE_MITM, std::to_underlying(configuration->Permission), "Subscribing to a MITM protected characteristic does not need MITM protection.");
    manager.DeleteAllServices();

    //
    //  A descriptor the configuration declares is protected too, and other descriptors are left alone.
    //
    const std::string descriptors = Descriptor(CLIENT_CHARACTERISTIC_CONFIGURATION_UUID, PERMISSION_READ | PERMISSION_WRITE, 2) + ',' + Descriptor(USER_DESCRIPTION_UUID, PERMISSION_READ, 8);
    characteristic = BuildOne(manager, Configuration("", PERMISSION_READ_ENCRYPTED | PERMISSION_WRITE_ENCRYPTED, PROPERTY_READ | PROPERTY_NOTIFY, descriptors));
    configuration = FindDescriptor(characteristic, CLIENT_CHARACTERISTIC_CONFIGURATION_UUID);
    TEST_ASSERT_NOT_NULL(configuration);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(plain | PERMISSION_WRITE_ENCRYPTED, std::to_underlying(configuration->Permission), "A declared client configuration was not protected.");
    BluetoothDescriptor *description = FindDescriptor(characteristic, USER_DESCRIPTION_UUID);
    TEST_ASSERT_NOT_NULL(description);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(PERMISSION_READ, std::to_underlying(description->Permission), "A descriptor other than the client configuration was changed.");
    manager.DeleteAllServices();
}

TEST_CASE("Permissions are classified by the security they need", CLASS_OF_TESTS)
{
    struct
    {
        uint16_t Permission;
        bool Encryption;
        bool Mitm;
    } const cases[] = {
        {.Permission = PERMISSION_READ | PERMISSION_WRITE, .Encryption = false, .Mitm = false},
        {.Permission = PERMISSION_READ_ENCRYPTED, .Encryption = true, .Mitm = false},
        {.Permission = PERMISSION_WRITE_ENCRYPTED, .Encryption = true, .Mitm = false},
        {.Permission = PERMISSION_READ_MITM, .Encryption = true, .Mitm = true},
        {.Permission = PERMISSION_WRITE_MITM, .Encryption = true, .Mitm = true},
        {.Permission = std::to_underlying(CharacteristicPermission::WriteSignedMITM), .Encryption = true, .Mitm = true},
    };

    for (const auto &item: cases)
    {
        const auto permission = static_cast<CharacteristicPermission>(item.Permission);
        TEST_ASSERT_EQUAL_MESSAGE(item.Encryption, BluetoothSecurityConfiguration::PermissionRequiresEncryption(permission), "Encryption was misjudged.");
        TEST_ASSERT_EQUAL_MESSAGE(item.Mitm, BluetoothSecurityConfiguration::PermissionRequiresMitm(permission), "MITM protection was misjudged.");
    }
}

TEST_CASE("A passkey has at most six digits", CLASS_OF_TESTS)
{
    TEST_ASSERT_TRUE(BluetoothSecurityConfiguration::IsValidPasskey(0));
    TEST_ASSERT_TRUE(BluetoothSecurityConfiguration::IsValidPasskey(999999));
    TEST_ASSERT_FALSE(BluetoothSecurityConfiguration::IsValidPasskey(1000000));
    TEST_ASSERT_FALSE(BluetoothSecurityConfiguration::IsValidPasskey(UINT32_MAX));
}
