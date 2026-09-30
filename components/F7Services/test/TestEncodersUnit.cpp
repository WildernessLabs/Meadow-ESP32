/*
 *  TestEncodersUnit.cpp
 *
 *  Implementation of the test methods for the low-level Encoders decode helpers.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "unity.h"

#include "Encoders.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Encoders][Emulated]"

/**
 * @brief Check that ExtractString decodes a normally terminated string.
 */
TEST_CASE("ExtractString decodes a terminated string", CLASS_OF_TESTS)
{
    const uint8_t * const buffer = reinterpret_cast<const uint8_t *>("hello");

    //
    //  maxLength must cover the terminator itself, so it is strlen("hello") + 1.
    //
    char *result = Encoders::ExtractString(buffer, 6);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    Utils::DeleteAndNullifyPointer(result);
}

/**
 * @brief Check that ExtractString returns nullptr for an empty string, matching the
 *        convention EncodeString uses to round-trip a nullptr as a single 0 byte.
 */
TEST_CASE("ExtractString returns nullptr for an empty string", CLASS_OF_TESTS)
{
    const uint8_t buffer[] = {0};

    char *result = Encoders::ExtractString(buffer, sizeof(buffer));
    TEST_ASSERT_NULL(result);
}

/**
 * @brief Check that a terminator sitting inside maxLength is found normally.
 */
TEST_CASE("ExtractString finds a terminator inside maxLength", CLASS_OF_TESTS)
{
    uint8_t buffer[20];
    memset(buffer, 'A', sizeof(buffer));
    buffer[10] = 0;

    char *result = Encoders::ExtractString(buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(10, strlen(result));
    Utils::DeleteAndNullifyPointer(result);
}

/**
 * @brief Check that ExtractString does not scan past maxLength when the buffer holds no
 *        terminator at all, and reports failure rather than returning an unterminated run
 *        of bytes as a string.
 *
 *  This is the regression check for issue #230: the buffer here is deliberately not
 *  0-terminated anywhere, including past maxLength, so an unbounded scan would walk off
 *  the end of this stack array.
 */
TEST_CASE("ExtractString returns nullptr when no terminator is found within maxLength", CLASS_OF_TESTS)
{
    const uint32_t maxLength = 16;
    uint8_t buffer[maxLength + 4];
    memset(buffer, 'A', sizeof(buffer));

    char *result = Encoders::ExtractString(buffer, maxLength);
    TEST_ASSERT_NULL(result);
}

/**
 * @brief Check that ExtractUInt16 decodes a value whose high byte has its top bit set.
 */
TEST_CASE("ExtractUInt16 decodes a value with the top bit set", CLASS_OF_TESTS)
{
    const uint8_t buffer[] = {0x34, 0xf2};

    TEST_ASSERT_EQUAL_HEX16(0xf234, Encoders::ExtractUInt16(buffer));
}

/**
 * @brief Check that ExtractUInt32 decodes every possible high byte.
 *
 *  Regression check for issue #274: shifting a byte of 0x80 or more left by 24 overflowed
 *  a signed int, which is undefined behaviour, because the byte was promoted to int rather
 *  than uint32_t before the shift.
 */
TEST_CASE("ExtractUInt32 decodes every high byte", CLASS_OF_TESTS)
{
    for (uint32_t highByte = 0; highByte <= 0xff; highByte++)
    {
        const uint32_t expected = (highByte << 24) | 0x00a5b6c7;
        uint8_t buffer[sizeof(uint32_t)];
        Encoders::EncodeUInt32(expected, buffer);
        TEST_ASSERT_EQUAL_HEX32(expected, Encoders::ExtractUInt32(buffer));
    }

    const uint8_t allOnes[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_HEX32(0xffffffff, Encoders::ExtractUInt32(allOnes));
}

/**
 * @brief Check that ExtractInt32 round trips negative values and the extremes.
 *
 *  Regression check for issue #274: ExtractInt32 has the same shift as ExtractUInt32, and
 *  it also accumulated into an int32_t while returning a uint32_t.
 */
TEST_CASE("ExtractInt32 round trips negative values and the extremes", CLASS_OF_TESTS)
{
    const int32_t values[] = {0, 1, -1, -2, 0x12345678, -0x12345678, INT32_MAX, INT32_MIN};

    for (const int32_t value: values)
    {
        uint8_t buffer[sizeof(int32_t)];
        Encoders::EncodeInt32(value, buffer);
        TEST_ASSERT_EQUAL_INT32(value, static_cast<int32_t>(Encoders::ExtractInt32(buffer)));
    }

    const uint8_t minusTwo[] = {0xfe, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_INT32(-2, static_cast<int32_t>(Encoders::ExtractInt32(minusTwo)));
}

/**
 * @brief Write a 32-bit value into a payload being built and move past it.
 *
 * @param position Where to write, advanced by four bytes.
 * @param value Value to write.
 */
static void PutUInt32(uint8_t *&position, const uint32_t value)
{
    Encoders::EncodeUInt32(value, position);
    position += 4;
}

/**
 * @brief Write the fixed part of a sendmsg request: socket, flags, header length and name length.
 *
 * @param position Where to write, advanced past the fields.
 * @param nameLength Name length to declare.
 */
static void PutSendMsgHeader(uint8_t *&position, const uint32_t nameLength)
{
    PutUInt32(position, 54);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, nameLength);
}

/**
 * @brief Decode a payload and check it is refused as malformed.
 *
 * @param payload Encoded request.
 * @param length Number of bytes in the payload.
 */
static void CheckSendMsgIsMalformed(const uint8_t *payload, const uint32_t length)
{
    bool malformed = false;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, length, &malformed);
    TEST_ASSERT_NULL_MESSAGE(request, "A request declaring more than its payload holds was decoded.");
    TEST_ASSERT_TRUE_MESSAGE(malformed, "The refusal was not reported as a malformed request.");
}

/**
 * @brief A well-formed sendmsg request with a name, two iovecs and control data decodes intact.
 */
TEST_CASE("ExtractSendMsgRequest decodes a well-formed request", CLASS_OF_TESTS)
{
    uint8_t payload[128] = {};
    uint8_t *position = payload;
    const uint8_t name[4] = {0x11, 0x22, 0x33, 0x44};
    PutSendMsgHeader(position, sizeof(name));
    memcpy(position, name, sizeof(name));
    position += sizeof(name);
    PutUInt32(position, 2);
    PutUInt32(position, 3);
    PutUInt32(position, 2);
    memcpy(position, "abcde", 5);
    position += 5;
    PutUInt32(position, 2);
    memcpy(position, "zz", 2);
    position += 2;
    PutUInt32(position, 7);

    bool malformed = true;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, static_cast<uint32_t>(position - payload), &malformed);
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_FALSE(malformed);
    TEST_ASSERT_EQUAL_INT32(54, request->SocketHandle);
    TEST_ASSERT_EQUAL_UINT32(sizeof(name), request->NameLength);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(name, request->Name, sizeof(name));
    TEST_ASSERT_EQUAL_UINT32(5, request->BufferLength);
    TEST_ASSERT_EQUAL_MEMORY("abcde", request->Buffer, 5);
    TEST_ASSERT_EQUAL_UINT32(2, request->ControlLength);
    TEST_ASSERT_EQUAL_MEMORY("zz", request->Control, 2);
    TEST_ASSERT_EQUAL_INT32(7, request->MsgFlags);
    Encoders::FreeSendMsgRequest(&request);
}

/**
 * @brief An iovec count the payload cannot hold is refused before the lengths are walked.
 *
 *  Regression check for issue #217: a count of 0xFFFFFFFF used to walk the address space
 *  four bytes at a time until the device faulted.
 */
TEST_CASE("ExtractSendMsgRequest refuses an iovec count the payload cannot hold", CLASS_OF_TESTS)
{
    uint8_t payload[32] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0xffffffff);
    PutUInt32(position, 4);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief iovec lengths whose 32-bit sum wraps to a small value are refused.
 *
 *  0xFFFFFFF8 + 0x10 wraps to 8 in 32 bits, which used to allocate 8 bytes while the
 *  request claimed four gigabytes of data.
 */
TEST_CASE("ExtractSendMsgRequest refuses iovec lengths that wrap when summed", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 2);
    PutUInt32(position, 0xfffffff8);
    PutUInt32(position, 0x10);
    position += 8;
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief A name, data or control length longer than the payload is refused.
 */
TEST_CASE("ExtractSendMsgRequest refuses lengths longer than the payload", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 1000);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));

    position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 1);
    PutUInt32(position, 1000);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));

    position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 1000);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief A payload that ends part way through a field is refused.
 */
TEST_CASE("ExtractSendMsgRequest refuses a truncated payload", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    const uint32_t complete = static_cast<uint32_t>(position - payload);

    bool malformed = true;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, complete, &malformed);
    TEST_ASSERT_NOT_NULL_MESSAGE(request, "The complete empty request was refused.");
    TEST_ASSERT_FALSE(malformed);
    Encoders::FreeSendMsgRequest(&request);

    for (uint32_t length = 0; length < complete; length++)
    {
        CheckSendMsgIsMalformed(payload, length);
    }
}

/**
 * @brief Value written to every byte of a buffer before an encoder is run over it, so a
 *        write past the space the encoder was given can be seen afterwards.
 */
static const uint8_t GUARD_BYTE = 0xa5;

/**
 * @brief Number of guard bytes placed after the space an encoder is given.
 */
static const uint32_t GUARD_LENGTH = 16;

/**
 * @brief Check that the guard bytes following the first length bytes of buffer are intact.
 *
 * @param buffer Buffer the encoder was run over.
 * @param length Number of bytes the encoder was allowed to write.
 * @param message Message to report if a guard byte has been overwritten.
 */
static void AssertGuardIntact(const uint8_t * const buffer, const uint32_t length, const char * const message)
{
    for (uint32_t index = 0; index < GUARD_LENGTH; index++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(GUARD_BYTE, buffer[length + index], message);
    }
}

/**
 * @brief Run an encoder over every buffer length from 0 up to the length its sizing
 *        function asked for, and check it refuses each one that is too short without
 *        writing past it, and accepts the full length.
 *
 * @param requiredLength Length returned by the matching Encoded*BufferSize function.
 * @param encode Callable taking (uint8_t *buffer, uint32_t bufferLength) and returning the encoder's result.
 * @param encoded Buffer of at least requiredLength bytes to receive the full-length encoding for the caller to decode.
 */
template<typename Encoder> static void CheckEncoderStaysInsideBuffer(const uint32_t requiredLength, Encoder encode, uint8_t * const encoded)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(requiredLength + GUARD_LENGTH));
    TEST_ASSERT_NOT_NULL(buffer);

    for (uint32_t length = 0; length < requiredLength; length++)
    {
        memset(buffer, GUARD_BYTE, requiredLength + GUARD_LENGTH);
        TEST_ASSERT_FALSE_MESSAGE(encode(buffer, length), "A buffer shorter than the sizing function asked for was accepted.");
        AssertGuardIntact(buffer, length, "The encoder wrote past the length it was given.");
    }

    memset(buffer, GUARD_BYTE, requiredLength + GUARD_LENGTH);
    TEST_ASSERT_TRUE_MESSAGE(encode(buffer, requiredLength), "The length the sizing function asked for was refused.");
    AssertGuardIntact(buffer, requiredLength, "The encoder wrote past the length its sizing function asked for.");
    memcpy(encoded, buffer, requiredLength);

    Utils::DeleteAndNullifyPointer(buffer);
}

/**
 * @brief EncodeString writes a string and its terminator that fit exactly, and moves the
 *        cursor and the remaining count past them.
 */
TEST_CASE("EncodeString writes a string that fits and advances past it", CLASS_OF_TESTS)
{
    uint8_t buffer[8];
    memset(buffer, GUARD_BYTE, sizeof(buffer));
    uint8_t *position = buffer;
    uint32_t remaining = 6;

    TEST_ASSERT_TRUE(Encoders::EncodeString("hello", position, remaining));
    TEST_ASSERT_EQUAL_STRING("hello", reinterpret_cast<char *>(buffer));
    TEST_ASSERT_EQUAL_PTR(buffer + 6, position);
    TEST_ASSERT_EQUAL_UINT32(0, remaining);
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, buffer[6]);
}

/**
 * @brief EncodeString writes nothing, and leaves the cursor and remaining count alone,
 *        when there is room for the characters but not the terminator.
 */
TEST_CASE("EncodeString writes nothing when the terminator does not fit", CLASS_OF_TESTS)
{
    uint8_t buffer[8];
    memset(buffer, GUARD_BYTE, sizeof(buffer));
    uint8_t *position = buffer;
    uint32_t remaining = 5;

    TEST_ASSERT_FALSE(Encoders::EncodeString("hello", position, remaining));
    TEST_ASSERT_EQUAL_PTR(buffer, position);
    TEST_ASSERT_EQUAL_UINT32(5, remaining);
    for (uint32_t index = 0; index < sizeof(buffer); index++)
    {
        TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, buffer[index]);
    }
}

/**
 * @brief EncodeString encodes nullptr as a single 0 byte, and refuses it when there is no room at all.
 */
TEST_CASE("EncodeString encodes nullptr as a single 0 byte", CLASS_OF_TESTS)
{
    uint8_t buffer[2];
    memset(buffer, GUARD_BYTE, sizeof(buffer));
    uint8_t *position = buffer;
    uint32_t remaining = 0;

    TEST_ASSERT_FALSE(Encoders::EncodeString(nullptr, position, remaining));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, buffer[0]);

    remaining = 1;
    TEST_ASSERT_TRUE(Encoders::EncodeString(nullptr, position, remaining));
    TEST_ASSERT_EQUAL_HEX8(0, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, buffer[1]);
    TEST_ASSERT_EQUAL_PTR(buffer + 1, position);
    TEST_ASSERT_EQUAL_UINT32(0, remaining);
}

/**
 * @brief EncodeSystemConfiguration refuses any buffer shorter than
 *        EncodedSystemConfigurationBufferSize asks for, and never writes past the one it is given.
 */
TEST_CASE("EncodeSystemConfiguration stays inside the buffer it is given", CLASS_OF_TESTS)
{
    Esp32Messaging::SystemConfiguration configuration = {};
    configuration.DeviceName = const_cast<char *>("MeadowDevice");
    configuration.DefaultAccessPoint = const_cast<char *>("Network");
    configuration.BuildBranchName = const_cast<char *>("develop");
    configuration.BuildHash = 0x12345678;

    const uint32_t requiredLength = Encoders::EncodedSystemConfigurationBufferSize(&configuration);
    uint8_t *encoded = static_cast<uint8_t *>(pvPortMalloc(requiredLength));
    TEST_ASSERT_NOT_NULL(encoded);
    CheckEncoderStaysInsideBuffer(requiredLength, [&configuration](uint8_t *buffer, const uint32_t bufferLength) { return (Encoders::EncodeSystemConfiguration(&configuration, buffer, bufferLength)); }, encoded);

    //
    //  The branch name is the last field, so it ends at the last byte.
    //
    TEST_ASSERT_EQUAL_STRING("develop", reinterpret_cast<char *>(encoded + requiredLength - strlen("develop") - 1));
    TEST_ASSERT_EQUAL_UINT32(0x12345678, Encoders::ExtractUInt32(encoded + requiredLength - strlen("develop") - 1 - 4));
    Utils::DeleteAndNullifyPointer(encoded);
}

/**
 * @brief EncodeAccessPointInformation refuses any buffer shorter than
 *        EncodedAccessPointInformationBufferSize asks for, and never writes past the one it is given.
 */
TEST_CASE("EncodeAccessPointInformation stays inside the buffer it is given", CLASS_OF_TESTS)
{
    Esp32Messaging::AccessPointInformation information = {};
    information.NetworkName = const_cast<char *>("Network");
    information.Password = const_cast<char *>("Password");
    information.Gateway = 0x0101a8c0;
    information.Hidden = 1;

    const uint32_t requiredLength = Encoders::EncodedAccessPointInformationBufferSize(&information);
    uint8_t *encoded = static_cast<uint8_t *>(pvPortMalloc(requiredLength));
    TEST_ASSERT_NOT_NULL(encoded);
    CheckEncoderStaysInsideBuffer(requiredLength, [&information](uint8_t *buffer, const uint32_t bufferLength) { return (Encoders::EncodeAccessPointInformation(&information, buffer, bufferLength)); }, encoded);

    Esp32Messaging::AccessPointInformation *decoded = Encoders::ExtractAccessPointInformation(encoded);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_STRING("Network", decoded->NetworkName);
    TEST_ASSERT_EQUAL_STRING("Password", decoded->Password);
    TEST_ASSERT_EQUAL_UINT32(0x0101a8c0, decoded->Gateway);
    TEST_ASSERT_EQUAL_UINT8(1, decoded->Hidden);
    Encoders::FreeAccessPointInformation(&decoded);
    Utils::DeleteAndNullifyPointer(encoded);
}

/**
 * @brief EncodeAddrInfo refuses any buffer shorter than EncodedAddrInfoBufferSize asks
 *        for, and never writes past the one it is given.
 */
TEST_CASE("EncodeAddrInfo stays inside the buffer it is given", CLASS_OF_TESTS)
{
    uint8_t address[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    Esp32Messaging::AddrInfo addrInfo = {};
    addrInfo.MyHeapAddress = 42;
    addrInfo.AddrLength = sizeof(address);
    addrInfo.Addr = address;
    addrInfo.CanonName = const_cast<char *>("host.example");
    addrInfo.Next = reinterpret_cast<void *>(0x1234);

    const uint32_t requiredLength = Encoders::EncodedAddrInfoBufferSize(&addrInfo);
    uint8_t *encoded = static_cast<uint8_t *>(pvPortMalloc(requiredLength));
    TEST_ASSERT_NOT_NULL(encoded);
    CheckEncoderStaysInsideBuffer(requiredLength, [&addrInfo](uint8_t *buffer, const uint32_t bufferLength) { return (Encoders::EncodeAddrInfo(&addrInfo, buffer, bufferLength)); }, encoded);

    Esp32Messaging::AddrInfo *decoded = Encoders::ExtractAddrInfo(encoded);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_UINT32(42, decoded->MyHeapAddress);
    TEST_ASSERT_EQUAL_UINT32(sizeof(address), decoded->AddrLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(address, decoded->Addr, sizeof(address));
    TEST_ASSERT_EQUAL_STRING("host.example", decoded->CanonName);
    Encoders::FreeAddrInfo(&decoded);
    Utils::DeleteAndNullifyPointer(encoded);
}

/**
 * @brief EncodeFileDetails refuses any buffer shorter than EncodedFileDetailsBufferSize
 *        asks for, and never writes past the one it is given.
 */
TEST_CASE("EncodeFileDetails stays inside the buffer it is given", CLASS_OF_TESTS)
{
    Esp32Messaging::FileDetails details = {};
    details.Name = const_cast<char *>("config.json");
    details.Length = 0x1234;

    const uint32_t requiredLength = Encoders::EncodedFileDetailsBufferSize(&details);
    uint8_t *encoded = static_cast<uint8_t *>(pvPortMalloc(requiredLength));
    TEST_ASSERT_NOT_NULL(encoded);
    CheckEncoderStaysInsideBuffer(requiredLength, [&details](uint8_t *buffer, const uint32_t bufferLength) { return (Encoders::EncodeFileDetails(&details, buffer, bufferLength)); }, encoded);

    Esp32Messaging::FileDetails *decoded = Encoders::ExtractFileDetails(encoded);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_STRING("config.json", decoded->Name);
    TEST_ASSERT_EQUAL_UINT16(0x1234, decoded->Length);
    Encoders::FreeFileDetails(&decoded);
    Utils::DeleteAndNullifyPointer(encoded);
}

/**
 * @brief EncodeFileNameAndContents refuses any buffer shorter than
 *        EncodedFileNameAndContentsBufferSize asks for, and never writes past the one it is given.
 */
TEST_CASE("EncodeFileNameAndContents stays inside the buffer it is given", CLASS_OF_TESTS)
{
    uint8_t contents[12] = {'f', 'i', 'l', 'e', ' ', 'c', 'o', 'n', 't', 'e', 'n', 't'};
    Esp32Messaging::FileNameAndContents file = {};
    file.Name = const_cast<char *>("data.bin");
    file.Contents = contents;
    file.ContentsLength = sizeof(contents);

    const uint32_t requiredLength = Encoders::EncodedFileNameAndContentsBufferSize(&file);
    uint8_t *encoded = static_cast<uint8_t *>(pvPortMalloc(requiredLength));
    TEST_ASSERT_NOT_NULL(encoded);
    CheckEncoderStaysInsideBuffer(requiredLength, [&file](uint8_t *buffer, const uint32_t bufferLength) { return (Encoders::EncodeFileNameAndContents(&file, buffer, bufferLength)); }, encoded);

    Esp32Messaging::FileNameAndContents *decoded = Encoders::ExtractFileNameAndContents(encoded);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_STRING("data.bin", decoded->Name);
    TEST_ASSERT_EQUAL_UINT32(sizeof(contents), decoded->ContentsLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(contents, decoded->Contents, sizeof(contents));
    Encoders::FreeFileNameAndContents(&decoded);
    Utils::DeleteAndNullifyPointer(encoded);
}
