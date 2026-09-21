/**
 * @file TestConfiguration.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the system configuration items.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "MockMessageDispatcher.hpp"
#include "NvsManager.hpp"
#include "SharedEnums.hpp"
#include "SystemRequestHandler.hpp"
#include "Utils.hpp"
#include "WiFiRequestHandler.hpp"

#include "TestAssertions.hpp"
#include "TestHarness.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  These cover the three messages the STM32 uses to work with the configuration:
 *  SetConfigurationItem, GetConfiguration and the configuration the ESP32 pushes at
 *  startup with SendConfigurationAsInterruptMessage.  Setting an item only writes NVS or
 *  a handler state variable and reading one only gathers those back up again, so nothing
 *  here needs a radio and the whole file runs under QEMU.
 *
 *  They are deliberately not tagged [Emulated], for two reasons.  Answering a System
 *  message builds a SystemRequestHandler, and that is a singleton whose constructor
 *  mounts the file system; running these alongside the [FileSystem] tests would leave a
 *  partition mounted underneath them.  Claiming the singleton also means any later test
 *  in the same boot that wants one of its own would reset the board.
 *
 *  run-tests-ci.sh therefore runs [Configuration] as a selector of its own, after
 *  [Emulated] and before the core dump tests, the first of which restarts the device and
 *  so hands the singletons back to whatever runs next.
 *
 *  Every test here puts back the value it found.  The settings are held in NVS and
 *  outlive the run, and the [WiFi] tests on the bench board read some of the same ones.
 */
#define CLASS_OF_TESTS "[Configuration]"

/**
 * @brief Milliseconds to wait for a configuration request to be answered.
 */
#define RESPONSE_TIMEOUT 5000

/**
 * @brief Number of bytes the item number and the value length occupy in the payload.
 */
#define CONFIGURATION_VALUE_HEADER_SIZE 8

/**
 * @brief An item number that is not in the ConfigurationItems enumeration.
 */
#define UNKNOWN_CONFIGURATION_ITEM 0x7fffffff

/**
 * @brief Number of bytes in a MAC address.
 */
#define MAC_ADDRESS_LENGTH 6

/**
 * @brief Name the maximum message queue length is stored under in NVS.
 *
 * @note MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME, DEVICE_NAME_NAME and MAXIMUM_RETRY_COUNT_NAME
 *       are all private to the handlers that own them, so the names are repeated here.
 *       Reading the configuration directly is what shows a value reached NVS and not
 *       just the static it is cached in.
 */
#define MAXIMUM_MESSAGE_QUEUE_LENGTH_STORAGE_NAME "MaxQueueLen"

/**
 * @brief Name the device name is stored under in NVS.
 */
#define DEVICE_NAME_STORAGE_NAME "DeviceName"

/**
 * @brief Name the maximum retry count is stored under in NVS.
 */
#define RETRY_COUNT_STORAGE_NAME "MaxRetry"

/**
 * @brief Device name the tests set, chosen not to be the default of MeadowF7.
 */
#define DEVICE_NAME_UNDER_TEST "TestMeadow"

/**
 * @brief Country code that allows hidden access points to be found and joined.
 */
#define COUNTRY_CODE_HIDDEN_APS "GB"

/**
 * @brief A code that is not in the list of valid country codes.
 */
#define COUNTRY_CODE_INVALID "ZZ"

/**
 * @brief Access point name the tests set.
 */
#define ACCESS_POINT_UNDER_TEST "ConfigurationTestAp"

/**
 * @brief Access point password the tests set.
 */
#define PASSWORD_UNDER_TEST "ConfigurationTestPassword"

/**
 * @brief Fixed part of an encoded SystemConfiguration, in bytes.
 *
 * @note This is the same number Encoders::EncodedSystemConfigurationBufferSize adds to
 *       the length of the three strings it carries.  It is repeated here so that a test
 *       checks the size against the encoding rather than against the method that works
 *       it out.
 */
#define ENCODED_CONFIGURATION_FIXED_SIZE 54

/**
 * @brief The parts of an encoded SystemConfiguration that these tests look at.
 *
 * @note The strings point into the message payload rather than being copied, so a decoded
 *       configuration is only valid for as long as the message it came from.
 */
struct DecodedConfiguration
{
    /**
     * @brief Maximum length of a message queue.
     */
    uint8_t MaximumMessageQueueLength;

    /**
     * @brief Maximum number of times a connection is retried.
     */
    int32_t MaximumRetryCount;

    /**
     * @brief Antenna currently selected.
     */
    uint8_t Antenna;

    /**
     * @brief MAC address of the station interface.
     */
    const uint8_t *BoardMacAddress;

    /**
     * @brief MAC address of the access point interface.
     */
    const uint8_t *SoftApMacAddress;

    /**
     * @brief MAC address of the Bluetooth interface.
     */
    const uint8_t *BluetoothMacAddress;

    /**
     * @brief Name of the device on the network.
     */
    const char *DeviceName;

    /**
     * @brief Access point the device connects to by default.
     */
    const char *DefaultAccessPoint;

    /**
     * @brief Reason the device last reset, in the STM32 numbering.
     */
    uint8_t ResetReason;

    /**
     * @brief Branch the firmware was built from.
     */
    const char *BuildBranchName;

    /**
     * @brief Number of bytes the configuration occupied in the payload.
     */
    uint32_t EncodedLength;
};

/**
 * @brief Dispatcher the tests send their requests through.
 *
 * @note This is the dispatcher the rest of the suite shares.  It is never released here:
 *       the SystemRequestHandler it builds is a singleton that claims itself for the rest
 *       of the boot, so a later dispatcher could not build another one.
 */
static MockMessageDispatcher *_dispatcher = nullptr;

/**
 * @brief Make sure the handlers the configuration messages need exist.
 *
 *  The System handler is built by the dispatcher when the first System message arrives.
 *  The WiFi handler has to be built here, because most of the configuration items are
 *  WiFi settings and the statics holding them are only filled in by its constructor; the
 *  country code in particular starts as a null pointer that SetCountryCode would compare
 *  against.  Setup() is deliberately not called on it: that creates a task and an event
 *  group, and starts nothing here needs.
 *
 * @return Pointer to the dispatcher the tests should use.
 */
static MockMessageDispatcher *EnsureHandlers()
{
    NvsManager::Setup();

    if (_dispatcher == nullptr)
    {
        _dispatcher = SetupMessageDispatcher();
        TEST_ASSERT_NOT_NULL(_dispatcher);
    }

    //
    //  The handler stores the country code in a static that starts as a null pointer and
    //  is filled in by the constructor, so a null says no handler has been built yet.
    //  GetInstance() would say the same thing but is private to the class.
    //
    if (WiFiRequestHandler::GetCountryCode() == nullptr)
    {
        WiFiRequestHandler *handler = new WiFiRequestHandler(_dispatcher);
        TEST_ASSERT_NOT_NULL(handler);
    }

    TEST_ASSERT_NOT_NULL_MESSAGE(WiFiRequestHandler::GetCountryCode(), "No WiFi request handler was created.");

    return (_dispatcher);
}

/**
 * @brief Build the payload for a SetConfigurationItem message.
 *
 * @param item Configuration item the value belongs to.
 * @param value Bytes making up the value, which may be a null pointer when length is 0.
 * @param valueLength Number of bytes in the value.
 * @param payloadLength Filled in with the number of bytes in the payload.
 *
 * @return Pointer to the payload, which the caller owns.
 */
static uint8_t *EncodeConfigurationItem(const uint32_t item, const void *value, const uint32_t valueLength, uint32_t *payloadLength)
{
    *payloadLength = CONFIGURATION_VALUE_HEADER_SIZE + valueLength;

    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(*payloadLength));
    TEST_ASSERT_NOT_NULL(payload);

    Encoders::EncodeUInt32(item, payload);
    Encoders::EncodeUInt32(valueLength, payload + 4);
    if (valueLength > 0)
    {
        memcpy(payload + CONFIGURATION_VALUE_HEADER_SIZE, value, valueLength);
    }

    return (payload);
}

/**
 * @brief Send a SetConfigurationItem message and return the status code it produced.
 *
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Number of bytes in the payload.
 *
 * @return Status code the ESP32 answered with.
 */
static StatusCodes::StatusCodes SendConfigurationItem(uint8_t *payload, const uint32_t payloadLength)
{
    MockMessageDispatcher *dispatcher = EnsureHandlers();

    Message *response = SendRequest(dispatcher, Esp32Interfaces::System, SystemFunction::SetConfigurationItem, payload, payloadLength, RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The SetConfigurationItem request was never answered.");

    StatusCodes::StatusCodes result = static_cast<StatusCodes::StatusCodes>(response->StatusCode);
    //
    //  The response carries the outcome in the status code and nothing else.
    //
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

    return (result);
}

/**
 * @brief Set a configuration item to a block of bytes.
 *
 * @param item Configuration item to set.
 * @param value Bytes making up the value.
 * @param valueLength Number of bytes in the value.
 *
 * @return Status code the ESP32 answered with.
 */
static StatusCodes::StatusCodes SetItem(const uint32_t item, const void *value, const uint32_t valueLength)
{
    uint32_t payloadLength = 0;
    uint8_t *payload = EncodeConfigurationItem(item, value, valueLength, &payloadLength);

    return (SendConfigurationItem(payload, payloadLength));
}

/**
 * @brief Set a configuration item that takes an unsigned integer.
 *
 * @param item Configuration item to set.
 * @param value Value to set it to.
 *
 * @return Status code the ESP32 answered with.
 */
static StatusCodes::StatusCodes SetUInt32Item(const uint32_t item, const uint32_t value)
{
    return (SetItem(item, &value, sizeof(value)));
}

/**
 * @brief Set a configuration item that takes a single byte.
 *
 * @param item Configuration item to set.
 * @param value Value to set it to.
 *
 * @return Status code the ESP32 answered with.
 */
static StatusCodes::StatusCodes SetByteItem(const uint32_t item, const uint8_t value)
{
    return (SetItem(item, &value, sizeof(value)));
}

/**
 * @brief Set a configuration item that takes a string.
 *
 * @param item Configuration item to set.
 * @param value Value to set it to, which is sent with its terminator.
 *
 * @return Status code the ESP32 answered with.
 */
static StatusCodes::StatusCodes SetStringItem(const uint32_t item, const char *value)
{
    return (SetItem(item, value, strlen(value) + 1));
}

/**
 * @brief Pull apart an encoded SystemConfiguration.
 *
 * @param buffer Payload holding the encoded configuration.
 * @param configuration Filled in with the values the payload carries.
 */
static void DecodeSystemConfiguration(const uint8_t *buffer, DecodedConfiguration *configuration)
{
    const uint8_t * const start = buffer;

    configuration->MaximumMessageQueueLength = *buffer;
    buffer += 1;
    configuration->MaximumRetryCount = Encoders::ExtractInt32(buffer);
    buffer += 4;
    configuration->Antenna = *buffer;
    buffer += 1;
    configuration->BoardMacAddress = buffer;
    buffer += MAC_ADDRESS_LENGTH;
    configuration->SoftApMacAddress = buffer;
    buffer += MAC_ADDRESS_LENGTH;
    configuration->BluetoothMacAddress = buffer;
    buffer += MAC_ADDRESS_LENGTH;
    configuration->DeviceName = reinterpret_cast<const char *>(buffer);
    buffer += strlen(configuration->DeviceName) + 1;
    configuration->DefaultAccessPoint = reinterpret_cast<const char *>(buffer);
    buffer += strlen(configuration->DefaultAccessPoint) + 1;
    configuration->ResetReason = *buffer;
    //
    //  The version numbers, the build date and time and the build hash sit between the
    //  reset reason and the branch name.  Nothing here asserts on them, so they are
    //  stepped over in one go: 1 reset reason, 4 version numbers of 4 bytes, 6 date and
    //  time bytes and a 4 byte hash.
    //
    buffer += 1 + (4 * 4) + 6 + 4;
    configuration->BuildBranchName = reinterpret_cast<const char *>(buffer);
    buffer += strlen(configuration->BuildBranchName) + 1;

    configuration->EncodedLength = static_cast<uint32_t>(buffer - start);
}

/**
 * @brief Ask for the system configuration and pull apart the answer.
 *
 * @param configuration Filled in with the values the response carries.
 *
 * @return Pointer to the response, which the caller owns and which the decoded strings
 *         point into.
 */
static Message *GetSystemConfiguration(DecodedConfiguration *configuration)
{
    MockMessageDispatcher *dispatcher = EnsureHandlers();

    Message *response = SendRequest(dispatcher, Esp32Interfaces::System, SystemFunction::GetConfiguration, nullptr, 0, RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The GetConfiguration request was never answered.");

    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(response);

    DecodeSystemConfiguration(response->Payload, configuration);

    return (response);
}

/**
 * @brief Treat a string that has never been set as an empty one.
 *
 *  The access point name and the password are held in statics that start as null
 *  pointers and are only filled in when a value is found in NVS, so a board that has
 *  never been given either reports a null rather than an empty string.
 *
 * @param value String to be checked.
 *
 * @return The string, or an empty string if it is a null pointer.
 */
static const char *SafeString(const char *value)
{
    return ((value == nullptr) ? "" : value);
}

/**
 * @brief Read a string configuration value straight out of NVS.
 *
 * @param name Name the value is stored under.
 * @param buffer Buffer to copy the value into.
 * @param bufferLength Number of bytes the buffer can hold.
 *
 * @return True if a value was found, false if nothing is stored under that name.
 */
static bool ReadStoredString(const char *name, char *buffer, const size_t bufferLength)
{
    const char *stored = SystemRequestHandler::GetConfigurationValue(name, nullptr);
    bool found = (stored != nullptr);

    if (found)
    {
        snprintf(buffer, bufferLength, "%s", stored);
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(const_cast<char **>(&stored)));
    }
    else
    {
        buffer[0] = '\0';
    }

    return (found);
}

/**
 * @brief Check that a request carrying no payload at all is refused.
 *
 *  The STM32 cannot say which item it means without one, so there is nothing sensible to
 *  do with the message other than refuse it.
 */
TEST_CASE("A configuration item with no payload is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SendConfigurationItem(nullptr, 0));
}

/**
 * @brief Check that a request too short to hold an item number and a length is refused.
 */
TEST_CASE("A configuration item with a short payload is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t shortPayloadLength = CONFIGURATION_VALUE_HEADER_SIZE - 1;
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(shortPayloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 0, shortPayloadLength);

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SendConfigurationItem(payload, shortPayloadLength));
}

/**
 * @brief Check that an item number the firmware does not know is refused.
 */
TEST_CASE("An unknown configuration item is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    TEST_ASSERT_EQUAL(StatusCodes::UnknownConfigurationItem, SetUInt32Item(UNKNOWN_CONFIGURATION_ITEM, 0));
}

/**
 * @brief Check that the items in the enumeration with no implementation are refused.
 *
 *  NtpServer, GetTimeAtStartup and BluetoothMacAddress are all in ConfigurationItems and
 *  none of them is handled.  They reach the default case and are refused in the same way
 *  as an item number that is not in the enumeration at all, which is what the STM32 has
 *  to be able to rely on until they are implemented.
 */
TEST_CASE("Configuration items that are not implemented are refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t unimplemented[] = {ConfigurationItems::NtpServer, ConfigurationItems::GetTimeAtStartup, ConfigurationItems::BluetoothMacAddress};

    for (uint32_t index = 0; index < (sizeof(unimplemented) / sizeof(unimplemented[0])); index++)
    {
        printf("Checking that configuration item %" PRIu32 " is refused\n", unimplemented[index]);
        TEST_ASSERT_EQUAL(StatusCodes::UnknownConfigurationItem, SetUInt32Item(unimplemented[index], 0));
    }
}

/**
 * @brief Check that an item carrying an empty value is refused.
 *
 *  Every item the switch handles reads the value the message carries, and a length of
 *  zero leaves nothing to read.  The message is refused before it gets that far.
 */
TEST_CASE("A configuration item with an empty value is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::DeviceName, nullptr, 0));
}

/**
 * @brief Check that a value length longer than the message is refused.
 *
 *  The length is sent by the STM32 and describes bytes that are supposed to follow it in
 *  the same message.  A length that runs past the end would otherwise be copied out of
 *  memory the message does not own.
 */
TEST_CASE("A configuration item whose value runs past the message is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t payloadLength = CONFIGURATION_VALUE_HEADER_SIZE + sizeof(uint32_t);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);

    Encoders::EncodeUInt32(ConfigurationItems::MaximumRetryCount, payload);
    //
    //  Four bytes of value are sent, and the length claims a great deal more.
    //
    Encoders::EncodeUInt32(1024, payload + 4);
    Encoders::EncodeUInt32(0, payload + CONFIGURATION_VALUE_HEADER_SIZE);

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SendConfigurationItem(payload, payloadLength));
}

/**
 * @brief Check that a fixed-size value shorter than the STM32 needs to read is refused.
 *
 *  DnsServer, StaticIpAddress, SubNetMask, DefaultGateway, MaximumMessageQueueLength and
 *  MaximumRetryCount all read a uint32_t out of the value, so a value shorter than 4 bytes
 *  would otherwise be read past its end.  DnsServer stands in for all six; they share the
 *  same check.
 */
TEST_CASE("A fixed-size configuration value shorter than a uint32_t is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint8_t shortValue[3] = {0x08, 0x08, 0x08};

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::DnsServer, shortValue, sizeof(shortValue)));
}

/**
 * @brief Check that a device name with no terminator within its value length is refused.
 *
 *  The value is a block of exactly ValueLength bytes copied from the STM32, with no
 *  guarantee it is terminated.  SetDeviceName calls strcmp on it, so an unterminated value
 *  has to be refused before it reaches that call rather than being read past its end.
 */
TEST_CASE("A device name with no terminator is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint8_t unterminated[4] = {'T', 'e', 's', 't'};

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::DeviceName, unterminated, sizeof(unterminated)));
}

/**
 * @brief Check that a country code with no terminator within its value length is refused.
 */
TEST_CASE("A country code with no terminator is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint8_t unterminated[2] = {'G', 'B'};

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::CountryCode, unterminated, sizeof(unterminated)));
}

/**
 * @brief Check that an access point name with no terminator within its value length is
 *        refused.
 *
 *  With no terminator, the handler cannot find where the access point name ends and the
 *  password begins, so the whole value has to be refused rather than treated as an access
 *  point name with an empty password.
 */
TEST_CASE("An access point name with no terminator is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint8_t unterminated[] = "NoTerminatorHere";

    //
    //  sizeof() includes the string literal's own terminator, so it is dropped from the
    //  value length to leave a value with no NUL anywhere in it.
    //
    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::DefaultApAndPassword, unterminated, sizeof(unterminated) - 1));
}

/**
 * @brief Check that an access point name with no room left for a password is refused.
 *
 *  The value carries only the access point name and its terminator, with nothing after
 *  it.  The handler must not read past the end of the value looking for a password that
 *  is not there.
 *
 * @note The access point name is applied before the missing password is discovered, so
 *       this test puts the original name and password back afterwards rather than leaving
 *       the failed request half-applied.
 */
TEST_CASE("An access point name with no password is refused", CLASS_OF_TESTS)
{
    EnsureHandlers();

    char originalAccessPoint[64];
    char originalPassword[96];
    snprintf(originalAccessPoint, sizeof(originalAccessPoint), "%s", SafeString(WiFiRequestHandler::GetDefaultAccessPoint()));
    snprintf(originalPassword, sizeof(originalPassword), "%s", SafeString(WiFiRequestHandler::GetPassword()));

    TEST_ASSERT_EQUAL(StatusCodes::InvalidPacket, SetItem(ConfigurationItems::DefaultApAndPassword, ACCESS_POINT_UNDER_TEST, strlen(ACCESS_POINT_UNDER_TEST) + 1));

    uint8_t value[sizeof(originalAccessPoint) + sizeof(originalPassword)];
    uint32_t valueLength = 0;
    valueLength += snprintf(reinterpret_cast<char *>(value), sizeof(value), "%s", originalAccessPoint) + 1;
    valueLength += snprintf(reinterpret_cast<char *>(value) + valueLength, sizeof(value) - valueLength, "%s", originalPassword) + 1;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetItem(ConfigurationItems::DefaultApAndPassword, value, valueLength));
    TEST_ASSERT_EQUAL_STRING(originalAccessPoint, WiFiRequestHandler::GetDefaultAccessPoint());
    TEST_ASSERT_EQUAL_STRING(originalPassword, WiFiRequestHandler::GetPassword());
}

/**
 * @brief Check that the maximum message queue length can be set.
 *
 *  The value is held in NVS so that it outlives a restart, and is read back by the
 *  SystemRequestHandler constructor on the next boot.
 */
TEST_CASE("The maximum message queue length can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t original = SystemRequestHandler::GetMaximumMessageQueueLength();
    printf("Maximum message queue length was %" PRIu32 " at the start of this test\n", original);
    const uint32_t underTest = original + 1;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::MaximumMessageQueueLength, underTest));
    TEST_ASSERT_EQUAL(underTest, SystemRequestHandler::GetMaximumMessageQueueLength());

    //
    //  The value reached NVS rather than only the static it is cached in.
    //
    TEST_ASSERT_EQUAL(underTest, SystemRequestHandler::GetConfigurationValue(MAXIMUM_MESSAGE_QUEUE_LENGTH_STORAGE_NAME, static_cast<uint32_t>(0)));

    //
    //  Setting the value it already holds is accepted and changes nothing.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::MaximumMessageQueueLength, underTest));
    TEST_ASSERT_EQUAL(underTest, SystemRequestHandler::GetMaximumMessageQueueLength());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::MaximumMessageQueueLength, original));
    TEST_ASSERT_EQUAL(original, SystemRequestHandler::GetMaximumMessageQueueLength());
}

/**
 * @brief Check that the maximum retry count can be set.
 *
 * @note A value of zero is reported as three by the getter, which is the behaviour it has
 *       always had, so the test stays clear of zero.  See TestWiFiConfiguration.cpp for
 *       the same value set through the WiFi handler rather than through a message.
 */
TEST_CASE("The maximum retry count can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t original = WiFiRequestHandler::GetMaximumRetryCount();
    printf("Maximum retry count was %" PRIu32 " at the start of this test\n", original);
    TEST_ASSERT_GREATER_THAN(0, original);
    const uint32_t underTest = original + 1;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::MaximumRetryCount, underTest));
    TEST_ASSERT_EQUAL(underTest, WiFiRequestHandler::GetMaximumRetryCount());
    TEST_ASSERT_EQUAL(underTest, SystemRequestHandler::GetConfigurationValue(RETRY_COUNT_STORAGE_NAME, static_cast<uint32_t>(0)));

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::MaximumRetryCount, original));
    TEST_ASSERT_EQUAL(original, WiFiRequestHandler::GetMaximumRetryCount());
}

/**
 * @brief Check that the device name can be set.
 *
 *  The name is held in NVS and is one of the values the STM32 reads back in the system
 *  configuration, so the test checks both the accessor and the stored value.
 */
TEST_CASE("The device name can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    char original[64];
    snprintf(original, sizeof(original), "%s", SafeString(SystemRequestHandler::GetDeviceName()));
    printf("Device name was %s at the start of this test\n", original);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::DeviceName, DEVICE_NAME_UNDER_TEST));
    TEST_ASSERT_EQUAL_STRING(DEVICE_NAME_UNDER_TEST, SystemRequestHandler::GetDeviceName());

    char stored[64];
    TEST_ASSERT_TRUE_MESSAGE(ReadStoredString(DEVICE_NAME_STORAGE_NAME, stored, sizeof(stored)), "The device name was not written to NVS.");
    TEST_ASSERT_EQUAL_STRING(DEVICE_NAME_UNDER_TEST, stored);

    //
    //  Setting the name it already holds is accepted and changes nothing.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::DeviceName, DEVICE_NAME_UNDER_TEST));
    TEST_ASSERT_EQUAL_STRING(DEVICE_NAME_UNDER_TEST, SystemRequestHandler::GetDeviceName());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::DeviceName, original));
    TEST_ASSERT_EQUAL_STRING(original, SystemRequestHandler::GetDeviceName());
}

/**
 * @brief Check that the default access point and its password can be set.
 *
 *  This is the one item that carries two values.  They are sent as two strings one after
 *  the other, each with its terminator, and the handler finds the password by stepping
 *  over the access point name.
 */
TEST_CASE("The default access point and password can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    //
    //  An SSID is at most 32 characters and a WPA passphrase at most 63, so these hold
    //  anything the board could have been given.  Whatever was there has to go back
    //  exactly as it was, so nothing here may truncate it.
    //
    char originalAccessPoint[64];
    char originalPassword[96];
    snprintf(originalAccessPoint, sizeof(originalAccessPoint), "%s", SafeString(WiFiRequestHandler::GetDefaultAccessPoint()));
    snprintf(originalPassword, sizeof(originalPassword), "%s", SafeString(WiFiRequestHandler::GetPassword()));
    printf("Default access point was %s at the start of this test\n", originalAccessPoint);

    uint8_t value[sizeof(originalAccessPoint) + sizeof(originalPassword)];
    uint32_t valueLength = 0;

    valueLength += snprintf(reinterpret_cast<char *>(value), sizeof(value), "%s", ACCESS_POINT_UNDER_TEST) + 1;
    valueLength += snprintf(reinterpret_cast<char *>(value) + valueLength, sizeof(value) - valueLength, "%s", PASSWORD_UNDER_TEST) + 1;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetItem(ConfigurationItems::DefaultApAndPassword, value, valueLength));
    TEST_ASSERT_EQUAL_STRING(ACCESS_POINT_UNDER_TEST, WiFiRequestHandler::GetDefaultAccessPoint());
    TEST_ASSERT_EQUAL_STRING(PASSWORD_UNDER_TEST, WiFiRequestHandler::GetPassword());

    //
    //  Put back whatever the board was using.  The [WiFi] tests take their credentials
    //  from the configuration file rather than from here, but the board still connects
    //  with these when the STM32 asks it to.
    //
    valueLength = 0;
    valueLength += snprintf(reinterpret_cast<char *>(value), sizeof(value), "%s", originalAccessPoint) + 1;
    valueLength += snprintf(reinterpret_cast<char *>(value) + valueLength, sizeof(value) - valueLength, "%s", originalPassword) + 1;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetItem(ConfigurationItems::DefaultApAndPassword, value, valueLength));
    TEST_ASSERT_EQUAL_STRING(originalAccessPoint, WiFiRequestHandler::GetDefaultAccessPoint());
    TEST_ASSERT_EQUAL_STRING(originalPassword, WiFiRequestHandler::GetPassword());
}

/**
 * @brief Check that the country code can be set and that an unknown one is refused.
 *
 * @note The two codes that matter are 01, which does not reach hidden access points, and
 *       GB, which does.  TestWiFiConfiguration.cpp covers the difference; this test is
 *       about the message reaching the setter and the refusal reaching the STM32.
 */
TEST_CASE("The country code can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    char original[8];
    snprintf(original, sizeof(original), "%s", WiFiRequestHandler::GetCountryCode());
    printf("Country code was %s at the start of this test\n", original);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::CountryCode, COUNTRY_CODE_HIDDEN_APS));
    TEST_ASSERT_EQUAL_STRING(COUNTRY_CODE_HIDDEN_APS, WiFiRequestHandler::GetCountryCode());

    //
    //  A code that is not in the valid list is refused and leaves the stored one alone.
    //
    TEST_ASSERT_EQUAL(StatusCodes::InvalidCountryCode, SetStringItem(ConfigurationItems::CountryCode, COUNTRY_CODE_INVALID));
    TEST_ASSERT_EQUAL_STRING(COUNTRY_CODE_HIDDEN_APS, WiFiRequestHandler::GetCountryCode());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::CountryCode, original));
    TEST_ASSERT_EQUAL_STRING(original, WiFiRequestHandler::GetCountryCode());
}

/**
 * @brief Check that the antenna can be selected and that an unknown one is refused.
 *
 *  Selecting an antenna drives two GPIO pins as well as writing NVS, so on the bench
 *  board this really does move the RF path.  The test puts the original antenna back
 *  before it finishes.
 */
TEST_CASE("The antenna can be selected", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint8_t original = WiFiRequestHandler::GetAntenna();
    printf("Antenna was %" PRIu8 " at the start of this test\n", original);
    TEST_ASSERT_LESS_OR_EQUAL(static_cast<uint8_t>(AntennaTypes::Max), original);

    const uint8_t underTest = (original == AntennaTypes::OnBoard) ? AntennaTypes::External : AntennaTypes::OnBoard;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::Antenna, underTest));
    TEST_ASSERT_EQUAL(underTest, WiFiRequestHandler::GetAntenna());

    //
    //  An antenna that is not one of the two the board has is refused and leaves the
    //  selected one alone.
    //
    TEST_ASSERT_EQUAL(StatusCodes::InvalidAntennaValue, SetByteItem(ConfigurationItems::Antenna, AntennaTypes::Max + 1));
    TEST_ASSERT_EQUAL(underTest, WiFiRequestHandler::GetAntenna());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::Antenna, original));
    TEST_ASSERT_EQUAL(original, WiFiRequestHandler::GetAntenna());
}

/**
 * @brief Check that the settings used when DHCP is turned off can be set.
 *
 *  These four are held in handler state rather than in NVS, so they last until the board
 *  restarts and no further.  They are applied when the interface is brought up, which is
 *  why nothing here needs the radio.
 */
TEST_CASE("The static network settings can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const uint32_t originalIpAddress = WiFiRequestHandler::GetIpAddress();
    const uint32_t originalSubNetMask = WiFiRequestHandler::GetSubNetMask();
    const uint32_t originalGateway = WiFiRequestHandler::GetDefaultGateway();
    const uint32_t originalDnsServer = WiFiRequestHandler::GetDnsServer();

    //
    //  192.168.1.100, 255.255.255.0, 192.168.1.1 and 8.8.8.8, in the order the bytes
    //  travel over the link.
    //
    const uint32_t ipAddress = 0x6401a8c0;
    const uint32_t subNetMask = 0x00ffffff;
    const uint32_t gateway = 0x0101a8c0;
    const uint32_t dnsServer = 0x08080808;

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::StaticIpAddress, ipAddress));
    TEST_ASSERT_EQUAL_HEX32(ipAddress, WiFiRequestHandler::GetIpAddress());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::SubNetMask, subNetMask));
    TEST_ASSERT_EQUAL_HEX32(subNetMask, WiFiRequestHandler::GetSubNetMask());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::DefaultGateway, gateway));
    TEST_ASSERT_EQUAL_HEX32(gateway, WiFiRequestHandler::GetDefaultGateway());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::DnsServer, dnsServer));
    TEST_ASSERT_EQUAL_HEX32(dnsServer, WiFiRequestHandler::GetDnsServer());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::StaticIpAddress, originalIpAddress));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::SubNetMask, originalSubNetMask));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::DefaultGateway, originalGateway));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetUInt32Item(ConfigurationItems::DnsServer, originalDnsServer));
}

/**
 * @brief Check that the settings that are either on or off can be set.
 *
 *  A value of 1 turns each of these on and anything else turns it off, which is what the
 *  handler makes of the single byte the STM32 sends.  Like the static network settings
 *  they are held in handler state rather than in NVS.
 */
TEST_CASE("The boolean network settings can be set", CLASS_OF_TESTS)
{
    EnsureHandlers();

    const bool originalUseDhcp = WiFiRequestHandler::GetUseDhcp();
    const bool originalAutomaticReconnect = WiFiRequestHandler::GetAutomaticReconnect();
    const bool originalAutomaticallyStartNetwork = WiFiRequestHandler::GetAutomaticallyStartNetwork();

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::UseDhcp, 1));
    TEST_ASSERT_TRUE(WiFiRequestHandler::GetUseDhcp());
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::UseDhcp, 0));
    TEST_ASSERT_FALSE(WiFiRequestHandler::GetUseDhcp());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyReconnect, 1));
    TEST_ASSERT_TRUE(WiFiRequestHandler::GetAutomaticReconnect());
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyReconnect, 0));
    TEST_ASSERT_FALSE(WiFiRequestHandler::GetAutomaticReconnect());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyStartNetwork, 1));
    TEST_ASSERT_TRUE(WiFiRequestHandler::GetAutomaticallyStartNetwork());
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyStartNetwork, 0));
    TEST_ASSERT_FALSE(WiFiRequestHandler::GetAutomaticallyStartNetwork());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::UseDhcp, originalUseDhcp ? 1 : 0));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyReconnect, originalAutomaticReconnect ? 1 : 0));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetByteItem(ConfigurationItems::AutomaticallyStartNetwork, originalAutomaticallyStartNetwork ? 1 : 0));
}

/**
 * @brief Check that setting a MAC address is accepted and changes nothing.
 *
 *  WiFiRequestHandler::SetMacAddress does nothing and reports success, so the STM32 can
 *  send one without being refused.  This test says so, so that the day it is implemented
 *  the test fails rather than the behaviour changing quietly.
 */
TEST_CASE("Setting a MAC address is accepted and changes nothing", CLASS_OF_TESTS)
{
    EnsureHandlers();

    uint8_t originalBoardMacAddress[MAC_ADDRESS_LENGTH];
    WiFiRequestHandler::GetMacAddress(originalBoardMacAddress, WIFI_IF_STA);

    const uint8_t macAddress[MAC_ADDRESS_LENGTH] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetItem(ConfigurationItems::BoardMacAddress, macAddress, sizeof(macAddress)));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetItem(ConfigurationItems::SoftApMacAddress, macAddress, sizeof(macAddress)));

    uint8_t boardMacAddress[MAC_ADDRESS_LENGTH];
    WiFiRequestHandler::GetMacAddress(boardMacAddress, WIFI_IF_STA);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(originalBoardMacAddress, boardMacAddress, MAC_ADDRESS_LENGTH);
}

/**
 * @brief Check that the system configuration can be read.
 *
 *  This is the message the STM32 sends when it wants everything at once.  The test checks
 *  that the payload is exactly as long as the encoding says it should be, and that the
 *  values in it agree with the accessors the rest of the firmware uses.
 */
TEST_CASE("The system configuration can be read", CLASS_OF_TESTS)
{
    EnsureHandlers();

    DecodedConfiguration configuration = {};
    Message *response = GetSystemConfiguration(&configuration);

    const uint32_t expectedLength = ENCODED_CONFIGURATION_FIXED_SIZE + strlen(configuration.DeviceName) + strlen(configuration.DefaultAccessPoint) + strlen(configuration.BuildBranchName);
    TEST_ASSERT_EQUAL_MESSAGE(expectedLength, response->PayloadLength, "The configuration payload is not the length the encoding gives it.");
    TEST_ASSERT_EQUAL_MESSAGE(response->PayloadLength, configuration.EncodedLength, "The decoded configuration did not use the whole payload.");

    TEST_ASSERT_EQUAL_STRING(SafeString(SystemRequestHandler::GetDeviceName()), configuration.DeviceName);
    TEST_ASSERT_EQUAL_STRING(SafeString(WiFiRequestHandler::GetDefaultAccessPoint()), configuration.DefaultAccessPoint);
    TEST_ASSERT_EQUAL(WiFiRequestHandler::GetMaximumRetryCount(), configuration.MaximumRetryCount);
    TEST_ASSERT_EQUAL(WiFiRequestHandler::GetAntenna(), configuration.Antenna);
    TEST_ASSERT_EQUAL(SystemRequestHandler::GetMaximumMessageQueueLength(), configuration.MaximumMessageQueueLength);

    //
    //  The station and Bluetooth addresses come from eFuse and are always available.  The
    //  access point address comes from the radio and is zero until the interface starts,
    //  so it is not checked here.
    //
    uint8_t boardMacAddress[MAC_ADDRESS_LENGTH];
    WiFiRequestHandler::GetMacAddress(boardMacAddress, WIFI_IF_STA);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(boardMacAddress, configuration.BoardMacAddress, MAC_ADDRESS_LENGTH);

    //
    //  The branch name is not asserted on.  build.sh writes it into build_info.h and puts
    //  the template back afterwards, so the test application is built from the template
    //  and reports an empty name; only the application build carries a real one.
    //
    printf("Device %s, branch '%s', antenna %" PRIu8 ", retry count %" PRIi32 "\n", configuration.DeviceName, configuration.BuildBranchName, configuration.Antenna, configuration.MaximumRetryCount);

    Message::DeleteMessage(response);
}

/**
 * @brief Check that a configuration item that has been set is reported back.
 *
 *  Setting an item and reading the configuration are two separate messages that go
 *  through two separate paths to the same state.  This is what says the two agree.
 */
TEST_CASE("A configuration item that was set is reported in the configuration", CLASS_OF_TESTS)
{
    EnsureHandlers();

    char original[64];
    snprintf(original, sizeof(original), "%s", SafeString(SystemRequestHandler::GetDeviceName()));

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::DeviceName, DEVICE_NAME_UNDER_TEST));

    DecodedConfiguration configuration = {};
    Message *response = GetSystemConfiguration(&configuration);
    TEST_ASSERT_EQUAL_STRING(DEVICE_NAME_UNDER_TEST, configuration.DeviceName);
    Message::DeleteMessage(response);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, SetStringItem(ConfigurationItems::DeviceName, original));

    response = GetSystemConfiguration(&configuration);
    TEST_ASSERT_EQUAL_STRING(original, configuration.DeviceName);
    Message::DeleteMessage(response);
}

/**
 * @brief Check the configuration the ESP32 pushes to the STM32 at startup.
 *
 *  AppMain calls this once the handlers are up, so that the STM32 has the configuration
 *  without having to ask for it.  It is an event rather than a response, and it carries
 *  the same encoded configuration that GetConfiguration answers with.
 */
TEST_CASE("The configuration is sent to the STM32 as an event", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = EnsureHandlers();

    //
    //  Anything already waiting would be taken for the event, so the queue starts empty.
    //
    DrainOutboundQueue(dispatcher, 0);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    SystemRequestHandler::SendConfigurationAsInterruptMessage();

    WaitForResponse(dispatcher, RESPONSE_TIMEOUT);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(event);
    //
    //  The message is built by hand rather than from a request, and the function it
    //  carries is left at zero, which is GetConfiguration.
    //
    ASSERT_EVENT(event, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(event);

    DecodedConfiguration configuration = {};
    DecodeSystemConfiguration(event->Payload, &configuration);
    TEST_ASSERT_EQUAL_MESSAGE(event->PayloadLength, configuration.EncodedLength, "The decoded configuration did not use the whole payload.");
    TEST_ASSERT_EQUAL_STRING(SafeString(SystemRequestHandler::GetDeviceName()), configuration.DeviceName);
    TEST_ASSERT_EQUAL(WiFiRequestHandler::GetAntenna(), configuration.Antenna);

    Message::DeleteMessage(event);
}
