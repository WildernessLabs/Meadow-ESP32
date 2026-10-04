/*
 *  TestEncodersMalformed.cpp
 *
 *  Tests that the decoders never read past the payload they are given, and refuse a payload
 *  that is too short for what it declares (issue #226).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "unity.h"

#include "Encoders.hpp"
#include "PayloadReader.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Encoders][Emulated]"

/**
 * @brief Builds an encoded payload a field at a time, as the STM32 does.
 */
class Payload
{
private:
    /**
     * @brief The bytes so far.
     */
    std::vector<uint8_t> _bytes;

public:
    /**
     * @brief Add one byte.
     */
    Payload &Byte(const uint8_t value)
    {
        _bytes.push_back(value);
        return (*this);
    }

    /**
     * @brief Add a 16-bit value, least significant byte first.
     */
    Payload &UInt16(const uint16_t value)
    {
        Byte(value & 0xff);
        return (Byte(value >> 8));
    }

    /**
     * @brief Add a 32-bit value, least significant byte first.
     */
    Payload &UInt32(const uint32_t value)
    {
        UInt16(value & 0xffff);
        return (UInt16(value >> 16));
    }

    /**
     * @brief Add a null terminated string.
     */
    Payload &Text(const char *value)
    {
        while (*value != 0)
        {
            Byte(static_cast<uint8_t>(*value++));
        }
        return (Byte(0));
    }

    /**
     * @brief Add a number of bytes with a recognisable value.
     */
    Payload &Fill(const uint32_t count)
    {
        for (uint32_t index = 0; index < count; index++)
        {
            Byte(static_cast<uint8_t>(0xa0 + index));
        }
        return (*this);
    }

    /**
     * @brief Overwrite a 32-bit value already added.
     */
    Payload &Overwrite(const uint32_t offset, const uint32_t value)
    {
        for (uint32_t index = 0; index < 4; index++)
        {
            _bytes[offset + index] = static_cast<uint8_t>(value >> (8 * index));
        }
        return (*this);
    }

    /**
     * @brief Number of bytes.
     */
    uint32_t Length() const
    {
        return (static_cast<uint32_t>(_bytes.size()));
    }

    /**
     * @brief The bytes.
     */
    const uint8_t *Bytes() const
    {
        return (_bytes.data());
    }
};

/**
 * @brief A decoder, wrapped so that every one can be called the same way.
 *
 * @param buffer Payload to decode.
 * @param length Number of bytes in it.
 * @param malformed Set to whether the payload was too short.
 *
 * @return true if the payload was decoded.
 */
typedef bool (*Decoder)(const uint8_t *buffer, const uint32_t length, bool *malformed);

/**
 * @brief Wrap a decoder whose result owns memory, released with its own Free function.
 */
#define OWNING_DECODER(Name)                                                                                                                                                                                                                                                                               \
    static bool Decode##Name(const uint8_t *buffer, const uint32_t length, bool *malformed)                                                                                                                                                                                                                \
    {                                                                                                                                                                                                                                                                                                      \
        Esp32Messaging::Name *result = Encoders::Extract##Name(buffer, length, malformed);                                                                                                                                                                                                                 \
        if (result == nullptr)                                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                                  \
            return (false);                                                                                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                                                                                  \
        Encoders::Free##Name(&result);                                                                                                                                                                                                                                                                     \
        return (true);                                                                                                                                                                                                                                                                                     \
    }

/**
 * @brief Wrap a decoder whose result owns nothing else.
 */
#define SIMPLE_DECODER(Name)                                                                                                                                                                                                                                                                               \
    static bool Decode##Name(const uint8_t *buffer, const uint32_t length, bool *malformed)                                                                                                                                                                                                                \
    {                                                                                                                                                                                                                                                                                                      \
        Esp32Messaging::Name *result = Encoders::Extract##Name(buffer, length, malformed);                                                                                                                                                                                                                 \
        if (result == nullptr)                                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                                  \
            return (false);                                                                                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                                                                                  \
        Utils::DeleteAndNullifyPointer(result);                                                                                                                                                                                                                                                            \
        return (true);                                                                                                                                                                                                                                                                                     \
    }

OWNING_DECODER(ConfigurationValue)
OWNING_DECODER(AccessPointInformation)
OWNING_DECODER(AddrInfo)
OWNING_DECODER(GetAddrInfoRequest)
OWNING_DECODER(ConnectRequest)
OWNING_DECODER(SetSockOptRequest)
OWNING_DECODER(WriteRequest)
OWNING_DECODER(SendRequest)
OWNING_DECODER(SendToRequest)
OWNING_DECODER(BindRequest)
OWNING_DECODER(BTStackConfig)
OWNING_DECODER(BTServerDataSet)
OWNING_DECODER(FileDetails)
OWNING_DECODER(FileNameAndContents)
OWNING_DECODER(LoggingConfiguration)
OWNING_DECODER(LogMessage)

SIMPLE_DECODER(DisconnectFromAccessPointRequest)
SIMPLE_DECODER(SockAddr)
SIMPLE_DECODER(SocketRequest)
SIMPLE_DECODER(FreeAddrInfoRequest)
SIMPLE_DECODER(TimeVal)
SIMPLE_DECODER(GetSockOptRequest)
SIMPLE_DECODER(ReadRequest)
SIMPLE_DECODER(CloseRequest)
SIMPLE_DECODER(RecvFromRequest)
SIMPLE_DECODER(RecvMsgRequest)
SIMPLE_DECODER(PollRequest)
SIMPLE_DECODER(ListenRequest)
SIMPLE_DECODER(AcceptRequest)
SIMPLE_DECODER(IoctlRequest)
SIMPLE_DECODER(GetSockPeerNameRequest)
SIMPLE_DECODER(SetAntennaRequest)
SIMPLE_DECODER(CoreDumpFragmentRequest)

/**
 * @brief A decoder and a payload it accepts.
 */
struct DecoderCase
{
    /**
     * @brief Name of the decoder, for the failure message.
     */
    const char *Name;

    /**
     * @brief The decoder.
     */
    Decoder Decode;

    /**
     * @brief A payload in the form the STM32 sends it.
     */
    Payload Valid;
};

/**
 * @brief Make a copy of a payload in a block of exactly its size, so that a read past the end is a read past the block.
 *
 * @param bytes Bytes to copy.
 * @param length Number of bytes.
 *
 * @return The copy, which the caller releases.
 */
static uint8_t *ExactCopy(const uint8_t *bytes, const uint32_t length)
{
    uint8_t *copy = static_cast<uint8_t *>(pvPortMalloc(length == 0 ? 1 : length));
    TEST_ASSERT_NOT_NULL(copy);
    if (length > 0)
    {
        memcpy(copy, bytes, length);
    }
    return (copy);
}

/**
 * @brief Check that a decoder accepts a payload, with bytes after it, and refuses every shorter one.
 *
 * @param testCase Decoder and a payload it accepts.
 */
static void CheckDecoder(const DecoderCase &testCase)
{
    char message[96];
    const uint32_t length = testCase.Valid.Length();

    bool malformed = true;
    uint8_t *whole = ExactCopy(testCase.Valid.Bytes(), length);
    snprintf(message, sizeof(message), "%s refused a valid payload.", testCase.Name);
    TEST_ASSERT_TRUE_MESSAGE(testCase.Decode(whole, length, &malformed), message);
    TEST_ASSERT_FALSE(malformed);
    vPortFree(whole);

    //
    //  Every shorter payload, each in a block of its own size, is refused as malformed.
    //
    for (uint32_t shorter = 0; shorter < length; shorter++)
    {
        uint8_t *prefix = ExactCopy(testCase.Valid.Bytes(), shorter);
        malformed = false;
        snprintf(message, sizeof(message), "%s accepted %u of its %u bytes.", testCase.Name, static_cast<unsigned>(shorter), static_cast<unsigned>(length));
        TEST_ASSERT_FALSE_MESSAGE(testCase.Decode(prefix, shorter, &malformed), message);
        snprintf(message, sizeof(message), "%s did not report %u of %u bytes as malformed.", testCase.Name, static_cast<unsigned>(shorter), static_cast<unsigned>(length));
        TEST_ASSERT_TRUE_MESSAGE(malformed, message);
        vPortFree(prefix);
    }

    //
    //  The decoders ignore what follows the fields they read.
    //
    Payload longer = testCase.Valid;
    longer.Fill(8);
    malformed = true;
    snprintf(message, sizeof(message), "%s refused a payload with bytes after it.", testCase.Name);
    TEST_ASSERT_TRUE_MESSAGE(testCase.Decode(longer.Bytes(), longer.Length(), &malformed), message);
    TEST_ASSERT_FALSE(malformed);
}

TEST_CASE("Every decoder accepts its payload and refuses every shorter one", CLASS_OF_TESTS)
{
    const DecoderCase cases[] = {
        {"ConfigurationValue", DecodeConfigurationValue, Payload().UInt32(1).UInt32(3).Fill(3)},
        {"AccessPointInformation", DecodeAccessPointInformation, Payload().Text("ssid").Text("password").UInt32(1).UInt32(2).UInt32(3).Byte(4).Byte(5).Byte(6)},
        {"DisconnectFromAccessPointRequest", DecodeDisconnectFromAccessPointRequest, Payload().Byte(1)},
        {"SockAddr", DecodeSockAddr, Payload().Byte(2).UInt16(80).UInt32(1).UInt32(2).Fill(16).UInt32(3)},
        {"AddrInfo", DecodeAddrInfo, Payload().UInt32(1).UInt32(2).UInt32(3).UInt32(4).UInt32(5).UInt32(6).UInt32(4).Fill(4).Text("canonical").UInt32(7)},
        {"GetAddrInfoRequest", DecodeGetAddrInfoRequest, Payload().Text("node").Text("service").UInt32(3).Fill(3)},
        {"SocketRequest", DecodeSocketRequest, Payload().UInt32(1).UInt32(2).UInt32(3).UInt32(4)},
        {"ConnectRequest", DecodeConnectRequest, Payload().UInt32(1).UInt32(4).Fill(4)},
        {"FreeAddrInfoRequest", DecodeFreeAddrInfoRequest, Payload().UInt32(1)},
        {"TimeVal", DecodeTimeVal, Payload().UInt32(1).UInt32(2)},
        {"SetSockOptRequest", DecodeSetSockOptRequest, Payload().UInt32(1).UInt32(2).UInt32(3).UInt32(4).Fill(4).UInt32(5)},
        {"GetSockOptRequest", DecodeGetSockOptRequest, Payload().UInt32(1).UInt32(2).UInt32(3)},
        {"WriteRequest", DecodeWriteRequest, Payload().UInt32(1).UInt32(2).Fill(2).UInt32(3)},
        {"ReadRequest", DecodeReadRequest, Payload().UInt32(1).UInt32(2)},
        {"CloseRequest", DecodeCloseRequest, Payload().UInt32(1)},
        {"SendRequest", DecodeSendRequest, Payload().UInt32(1).UInt32(2).Fill(2).UInt32(3).UInt32(4)},
        {"SendToRequest", DecodeSendToRequest, Payload().UInt32(1).UInt32(2).Fill(2).UInt32(3).UInt32(4).UInt32(3).Fill(3)},
        {"RecvFromRequest", DecodeRecvFromRequest, Payload().UInt32(1).UInt32(2).UInt32(3).UInt32(4)},
        {"RecvMsgRequest", DecodeRecvMsgRequest, Payload().UInt32(1).UInt32(2).UInt32(3).UInt32(4).UInt32(5)},
        {"PollRequest", DecodePollRequest, Payload().UInt32(1).UInt16(2).UInt32(3).UInt32(4).UInt32(5)},
        {"ListenRequest", DecodeListenRequest, Payload().UInt32(1).UInt32(2)},
        {"BindRequest", DecodeBindRequest, Payload().UInt32(1).UInt32(3).Fill(3)},
        {"AcceptRequest", DecodeAcceptRequest, Payload().UInt32(1)},
        {"IoctlRequest", DecodeIoctlRequest, Payload().UInt32(1)},
        {"GetSockPeerNameRequest", DecodeGetSockPeerNameRequest, Payload().UInt32(1)},
        {"SetAntennaRequest", DecodeSetAntennaRequest, Payload().Byte(1).Byte(0)},
        {"BTStackConfig", DecodeBTStackConfig, Payload().Text("{\"deviceName\":\"x\"}")},
        {"BTServerDataSet", DecodeBTServerDataSet, Payload().UInt16(1).UInt32(2).Fill(2)},
        {"FileDetails", DecodeFileDetails, Payload().Text("file.txt").UInt16(5)},
        {"FileNameAndContents", DecodeFileNameAndContents, Payload().Text("file.txt").UInt32(3).Fill(3)},
        {"LoggingConfiguration", DecodeLoggingConfiguration, Payload().Byte(1).UInt16(2).Text("WiFi")},
        {"LogMessage", DecodeLogMessage, Payload().Byte(1).Text("hello")},
        {"CoreDumpFragmentRequest", DecodeCoreDumpFragmentRequest, Payload().UInt32(1).UInt32(2)},
    };

    for (const DecoderCase &testCase: cases)
    {
        CheckDecoder(testCase);
    }
}

TEST_CASE("A declared length beyond the payload is refused before anything is allocated", CLASS_OF_TESTS)
{
    //
    //  A write of a file whose contents length is 0xFFFFFFF0 and one that is a little more than the
    //  payload holds: the first asks for about 4GB, the second is allocated and copied from past the
    //  end of the payload.
    //
    const uint32_t declared[] = {0xfffffff0, 0x80000000, 10, 4};
    for (const uint32_t contentsLength: declared)
    {
        Payload file;
        file.Text("file.txt").UInt32(contentsLength).Fill(3);
        uint8_t *exact = ExactCopy(file.Bytes(), file.Length());
        bool malformed = false;
        Esp32Messaging::FileNameAndContents *result = Encoders::ExtractFileNameAndContents(exact, file.Length(), &malformed);
        TEST_ASSERT_NULL_MESSAGE(result, "A file declaring more contents than it carries was decoded.");
        TEST_ASSERT_TRUE(malformed);
        vPortFree(exact);

        Payload value;
        value.UInt32(1).UInt32(contentsLength).Fill(3);
        exact = ExactCopy(value.Bytes(), value.Length());
        malformed = false;
        Esp32Messaging::ConfigurationValue *configuration = Encoders::ExtractConfigurationValue(exact, value.Length(), &malformed);
        TEST_ASSERT_NULL_MESSAGE(configuration, "A configuration value declaring more than it carries was decoded.");
        TEST_ASSERT_TRUE(malformed);
        vPortFree(exact);
    }
}

TEST_CASE("A decoder reports no payload as malformed", CLASS_OF_TESTS)
{
    bool malformed = false;
    TEST_ASSERT_NULL(Encoders::ExtractSocketRequest(nullptr, 0, &malformed));
    TEST_ASSERT_TRUE(malformed);

    malformed = false;
    TEST_ASSERT_NULL(Encoders::ExtractFileDetails(nullptr, 100, &malformed));
    TEST_ASSERT_TRUE(malformed);
}

TEST_CASE("PayloadReader fails once and stays failed", CLASS_OF_TESTS)
{
    const uint8_t bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    PayloadReader reader(bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL_UINT32(0x04030201, reader.ReadUInt32());
    TEST_ASSERT_FALSE(reader.Failed());
    TEST_ASSERT_EQUAL_UINT8(0x05, reader.ReadByte());

    //
    //  One byte is left and four are wanted: the read fails and returns 0, and so does every read after it.
    //
    TEST_ASSERT_EQUAL_UINT32(0, reader.ReadUInt32());
    TEST_ASSERT_TRUE(reader.Failed());
    TEST_ASSERT_FALSE(reader.OutOfMemory());
    TEST_ASSERT_EQUAL_UINT8(0, reader.ReadByte());
    TEST_ASSERT_NULL(reader.ReadBlock(1));
    TEST_ASSERT_NULL(reader.ReadString());
}

TEST_CASE("PayloadReader allocates nothing for a block longer than the payload", CLASS_OF_TESTS)
{
    const uint8_t bytes[] = {0x01, 0x02, 0x03};

    PayloadReader reader(bytes, sizeof(bytes));
    TEST_ASSERT_NULL(reader.ReadBlock(0xfffffff0));
    TEST_ASSERT_TRUE(reader.Failed());
    TEST_ASSERT_FALSE_MESSAGE(reader.OutOfMemory(), "A block longer than the payload was reported as running out of memory.");

    PayloadReader exact(bytes, sizeof(bytes));
    uint8_t *block = exact.ReadBlock(3);
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_MEMORY(bytes, block, sizeof(bytes));
    TEST_ASSERT_FALSE(exact.Failed());
    vPortFree(block);

    PayloadReader none(bytes, sizeof(bytes));
    TEST_ASSERT_NULL(none.ReadBlock(0));
    TEST_ASSERT_FALSE_MESSAGE(none.Failed(), "A block of no bytes failed the reader.");
}

TEST_CASE("PayloadReader reads strings only up to the end of the payload", CLASS_OF_TESTS)
{
    const uint8_t terminated[] = {'a', 'b', 0, 0, 'c', 0};
    PayloadReader reader(terminated, sizeof(terminated));
    char *first = reader.ReadString();
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_STRING("ab", first);
    vPortFree(first);
    //
    //  An empty string is not an error, it is nullptr, as it always was.
    //
    TEST_ASSERT_NULL(reader.ReadString());
    TEST_ASSERT_FALSE(reader.Failed());
    char *last = reader.ReadString();
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_EQUAL_STRING("c", last);
    vPortFree(last);
    TEST_ASSERT_FALSE(reader.Failed());

    //
    //  No terminator within the payload, in a block with nothing after it.
    //
    uint8_t *unterminated = ExactCopy(reinterpret_cast<const uint8_t *>("abcd"), 4);
    PayloadReader unfinished(unterminated, 4);
    TEST_ASSERT_NULL(unfinished.ReadString());
    TEST_ASSERT_TRUE(unfinished.Failed());
    TEST_ASSERT_FALSE(unfinished.OutOfMemory());
    vPortFree(unterminated);
}
