/*
 *  TestEncoderSizes.cpp
 *
 *  Tests that every Encoded*BufferSize function matches what its encoder writes (issue #268).
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
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
 * @brief Bytes after the sized buffer that the encoder must leave untouched.
 *
 *  Large enough that an encoder overrunning its size by any amount these tests can
 *  provoke writes into the guard rather than into the next heap block.
 */
#define GUARD_LENGTH 256

/**
 * @brief Length of each variable-length field the tests fill in.
 *
 *  Each field is a different length, so a sizer that counts one field twice, or the
 *  wrong field, gives the wrong total.
 */
#define FIRST_FIELD_LENGTH 7
#define SECOND_FIELD_LENGTH 11
#define THIRD_FIELD_LENGTH 13

/**
 * @brief Source of the bytes the variable-length fields point at.
 */
static uint8_t _fieldData[64];

/**
 * @brief Fill _fieldData with a recognisable pattern.
 */
static void FillFieldData()
{
    for (uint32_t index = 0; index < sizeof(_fieldData); index++)
    {
        _fieldData[index] = static_cast<uint8_t>(0x40 + index);
    }
}

/**
 * @brief Encode value into a buffer filled with fill, and report the last byte written.
 *
 * @param name Name of the encoder, for the failure messages.
 * @param length Length the sizer gave.
 * @param fill Byte the buffer and its guard are filled with first.
 * @param encode Writes the encoded value to the buffer it is given.
 *
 * @return Index of the last byte inside length that differs from fill, or -1 if none does.
 */
template<typename Encode> static int32_t EncodeIntoFilledBuffer(const char *name, const int32_t length, const uint8_t fill, Encode encode)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(length + GUARD_LENGTH));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, fill, length + GUARD_LENGTH);

    encode(buffer, static_cast<uint32_t>(length));

    for (int32_t index = length; index < (length + GUARD_LENGTH); index++)
    {
        if (buffer[index] != fill)
        {
            printf("%s wrote byte %" PRIi32 " of a %" PRIi32 " byte buffer.\n", name, index, length);
            Utils::DeleteAndNullifyPointer(buffer);
            TEST_FAIL_MESSAGE("The encoder wrote past the size its sizer gave.");
        }
    }

    int32_t lastWritten = -1;
    for (int32_t index = 0; index < length; index++)
    {
        if (buffer[index] != fill)
        {
            lastWritten = index;
        }
    }
    Utils::DeleteAndNullifyPointer(buffer);
    return (lastWritten);
}

/**
 * @brief Check that an encoder writes exactly the number of bytes its sizer gives.
 *
 *  The buffer is filled first with 0x00 and then with 0xff.  No byte equals both, so the
 *  last byte that differs from its fill in either run is the last byte the encoder wrote.
 *
 * @param name Name of the encoder, for the failure messages.
 * @param length Length the sizer gave.
 * @param encode Writes the encoded value to the buffer it is given.
 */
template<typename Encode> static void CheckEncodedSize(const char *name, const int32_t length, Encode encode)
{
    TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(0, length, name);
    const int32_t lastWithZeros = EncodeIntoFilledBuffer(name, length, 0x00, encode);
    const int32_t lastWithOnes = EncodeIntoFilledBuffer(name, length, 0xff, encode);
    const int32_t written = ((lastWithZeros > lastWithOnes) ? lastWithZeros : lastWithOnes) + 1;
    if (written != length)
    {
        printf("%s wrote %" PRIi32 " bytes; its sizer gave %" PRIi32 ".\n", name, written, length);
    }
    TEST_ASSERT_EQUAL_INT32_MESSAGE(length, written, "The encoder and its sizer disagree on the encoded length.");
}

/**
 * @brief Check a void Encode<Type> function against Encoded<Type>BufferSize.
 */
#define CHECK_ENCODER(Encoder, Sizer, value) CheckEncodedSize(#Encoder, Encoders::Sizer(&(value)), [&](uint8_t *buffer, uint32_t) { Encoders::Encoder(&(value), buffer); })

/**
 * @brief Check a bounded Encode<Type> function, which takes the buffer length, against its sizer.
 */
#define CHECK_BOUNDED_ENCODER(Encoder, Sizer, value) CheckEncodedSize(#Encoder, Encoders::Sizer(&(value)), [&](uint8_t *buffer, uint32_t length) { TEST_ASSERT_TRUE_MESSAGE(Encoders::Encoder(&(value), buffer, length), #Encoder " refused a buffer of the size its sizer gave."); })

/**
 * @brief Copy a string into a fixed-size SSID field, filling it.
 *
 * @param ssid Field to fill.
 * @param length Length of the field, including the terminator.
 */
static void FillSsid(char *ssid, const size_t length)
{
    memset(ssid, 'S', length - 1);
    ssid[length - 1] = 0;
}

TEST_CASE("WiFi event encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    Esp32Messaging::ConnectEventData connect = {};
    connect.IpAddress = 0x0101a8c0;
    connect.SubnetMask = 0x00ffffff;
    connect.Gateway = 0x0101a8c0;
    FillSsid(connect.Ssid, sizeof(connect.Ssid));
    memset(connect.Bssid, 0xb5, sizeof(connect.Bssid));
    connect.Channel = 11;
    connect.AuthenticationMode = 3;
    connect.Reason = 0x01020304;
    CHECK_ENCODER(EncodeConnectEventData, EncodedConnectEventDataBufferSize, connect);

    Esp32Messaging::DisconnectEventData disconnect = {};
    FillSsid(disconnect.Ssid, sizeof(disconnect.Ssid));
    disconnect.SsidLength = 32;
    memset(disconnect.Bssid, 0xb5, sizeof(disconnect.Bssid));
    disconnect.Rssi = -40;
    disconnect.Reason = 201;
    CHECK_ENCODER(EncodeDisconnectEventData, EncodedDisconnectEventDataBufferSize, disconnect);

    Esp32Messaging::NodeConnectionChangeEventData node = {};
    node.IpAddress = 0x0201a8c0;
    memset(node.MacAddress, 0x3c, sizeof(node.MacAddress));
    CHECK_ENCODER(EncodeNodeConnectionChangeEventData, EncodedNodeConnectionChangeEventDataBufferSize, node);

    Esp32Messaging::GotIpEventData gotIp = {};
    gotIp.DnsAddress = 0x0808a8c0;
    CHECK_ENCODER(EncodeGotIpEventData, EncodedGotIpEventDataBufferSize, gotIp);
}

TEST_CASE("Access point encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    FillFieldData();

    Esp32Messaging::AccessPoint accessPoint = {};
    FillSsid(accessPoint.Ssid, sizeof(accessPoint.Ssid));
    memset(accessPoint.Bssid, 0xb5, sizeof(accessPoint.Bssid));
    accessPoint.PrimaryChannel = 6;
    accessPoint.SecondaryChannel = 1;
    accessPoint.Rssi = -55;
    accessPoint.AuthenticationMode = 4;
    accessPoint.Protocols = 0x0a0b0c0d;
    CHECK_ENCODER(EncodeAccessPoint, EncodedAccessPointBufferSize, accessPoint);

    Esp32Messaging::AccessPointList list = {};
    list.NumberOfAccessPoints = 2;
    list.AccessPointsLength = FIRST_FIELD_LENGTH;
    list.AccessPoints = _fieldData;
    CHECK_ENCODER(EncodeAccessPointList, EncodedAccessPointListBufferSize, list);

    Esp32Messaging::AccessPointInformation information = {};
    information.NetworkName = const_cast<char *>("BenchNetwork");
    information.Password = const_cast<char *>("BenchPassword1");
    information.IpAddress = 0x0101a8c0;
    information.SubnetMask = 0x00ffffff;
    information.Gateway = 0x0101a8c0;
    information.WiFiAuthenticationMode = 3;
    information.Channel = 6;
    information.Hidden = 1;
    CHECK_BOUNDED_ENCODER(EncodeAccessPointInformation, EncodedAccessPointInformationBufferSize, information);
}

TEST_CASE("Socket address and lookup encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    FillFieldData();

    Esp32Messaging::SockAddr sockAddr = {};
    sockAddr.Family = 2;
    sockAddr.Port = 0x1f90;
    sockAddr.Ip4Address = 0x4101a8c0;
    sockAddr.FlowInfo = 0x01020304;
    memset(sockAddr.Ip6Address, 0x66, sizeof(sockAddr.Ip6Address));
    sockAddr.ScopeID = 0x05060708;
    CHECK_ENCODER(EncodeSockAddr, EncodedSockAddrBufferSize, sockAddr);

    Esp32Messaging::AddrInfo addrInfo = {};
    addrInfo.MyHeapAddress = 0x3ffb0000;
    addrInfo.Flags = 1;
    addrInfo.Family = 2;
    addrInfo.SocketType = 1;
    addrInfo.Protocol = 6;
    addrInfo.AddrLen = FIRST_FIELD_LENGTH;
    addrInfo.AddrLength = FIRST_FIELD_LENGTH;
    addrInfo.Addr = _fieldData;
    addrInfo.CanonName = const_cast<char *>("bench.local");
    CHECK_BOUNDED_ENCODER(EncodeAddrInfo, EncodedAddrInfoBufferSize, addrInfo);

    Esp32Messaging::GetAddrInfoResponse getAddrInfo = {};
    getAddrInfo.AddrInfoResponseErrno = 5;
    getAddrInfo.ResLength = SECOND_FIELD_LENGTH;
    getAddrInfo.Res = _fieldData;
    CHECK_ENCODER(EncodeGetAddrInfoResponse, EncodedGetAddrInfoResponseBufferSize, getAddrInfo);
}

TEST_CASE("Socket response encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    FillFieldData();

    Esp32Messaging::IntegerResponse integer = {};
    integer.Result = -1;
    CHECK_ENCODER(EncodeIntegerResponse, EncodedIntegerResponseBufferSize, integer);

    Esp32Messaging::IntegerAndErrnoResponse integerAndErrno = {};
    integerAndErrno.Result = -1;
    integerAndErrno.ResponseErrno = 104;
    CHECK_ENCODER(EncodeIntegerAndErrnoResponse, EncodedIntegerAndErrnoResponseBufferSize, integerAndErrno);

    Esp32Messaging::TimeVal timeVal = {};
    timeVal.TvSec = 0x01020304;
    timeVal.TvUsec = 0x05060708;
    CHECK_ENCODER(EncodeTimeVal, EncodedTimeValBufferSize, timeVal);

    Esp32Messaging::GetSockOptResponse getSockOpt = {};
    getSockOpt.Result = 0;
    getSockOpt.ResponseErrno = 22;
    getSockOpt.OptionValueLength = FIRST_FIELD_LENGTH;
    getSockOpt.OptionValue = _fieldData;
    getSockOpt.OptionLen = FIRST_FIELD_LENGTH;
    CHECK_ENCODER(EncodeGetSockOptResponse, EncodedGetSockOptResponseBufferSize, getSockOpt);

    Esp32Messaging::Linger linger = {};
    linger.LOnOff = 1;
    linger.LLinger = 0x01020304;
    CHECK_ENCODER(EncodeLinger, EncodedLingerBufferSize, linger);

    Esp32Messaging::ReadResponse read = {};
    read.BufferLength = SECOND_FIELD_LENGTH;
    read.Buffer = _fieldData;
    read.ReadResponseResult = SECOND_FIELD_LENGTH;
    read.ReadResponseErrno = 11;
    CHECK_ENCODER(EncodeReadResponse, EncodedReadResponseBufferSize, read);

    Esp32Messaging::RecvFromResponse recvFrom = {};
    recvFrom.BufferLength = FIRST_FIELD_LENGTH;
    recvFrom.Buffer = _fieldData;
    recvFrom.Result = FIRST_FIELD_LENGTH;
    recvFrom.ResponseErrno = 11;
    recvFrom.SourceAddressLength = SECOND_FIELD_LENGTH;
    recvFrom.SourceAddress = _fieldData;
    recvFrom.SourceAddressLen = SECOND_FIELD_LENGTH;
    CHECK_ENCODER(EncodeRecvFromResponse, EncodedRecvFromResponseBufferSize, recvFrom);

    Esp32Messaging::RecvMsgResponse recvMsg = {};
    recvMsg.BufferLength = FIRST_FIELD_LENGTH;
    recvMsg.Buffer = _fieldData;
    recvMsg.Result = FIRST_FIELD_LENGTH;
    recvMsg.ResponseErrno = 11;
    recvMsg.SourceAddressLength = SECOND_FIELD_LENGTH;
    recvMsg.SourceAddress = _fieldData;
    recvMsg.SourceAddressLen = SECOND_FIELD_LENGTH;
    recvMsg.ControlLength = THIRD_FIELD_LENGTH;
    recvMsg.Control = _fieldData;
    recvMsg.MsgFlags = 0x40;
    CHECK_ENCODER(EncodeRecvMsgResponse, EncodedRecvMsgResponseBufferSize, recvMsg);

    Esp32Messaging::PollResponse poll = {};
    poll.ReturnedEvents = 0x1234;
    poll.Result = 1;
    poll.ResponseErrno = 4;
    CHECK_ENCODER(EncodePollResponse, EncodedPollResponseBufferSize, poll);

    Esp32Messaging::InterruptPollResponse interruptPoll = {};
    interruptPoll.SocketHandle = 54;
    interruptPoll.Result = 1;
    interruptPoll.ResponseErrno = 4;
    interruptPoll.ReturnedEvents = 0x1234;
    interruptPoll.SetupMessageId = 0x01020304;
    CHECK_ENCODER(EncodeInterruptPollResponse, EncodedInterruptPollResponseBufferSize, interruptPoll);

    Esp32Messaging::AcceptResponse accept = {};
    accept.AddrLength = FIRST_FIELD_LENGTH;
    accept.Addr = _fieldData;
    accept.Result = 55;
    accept.ResponseErrno = 11;
    CHECK_ENCODER(EncodeAcceptResponse, EncodedAcceptResponseBufferSize, accept);

    Esp32Messaging::IoctlResponse ioctlResponse = {};
    ioctlResponse.AddrLength = SECOND_FIELD_LENGTH;
    ioctlResponse.Addr = _fieldData;
    ioctlResponse.Flags = 0x0800;
    ioctlResponse.Result = 0;
    ioctlResponse.ResponseErrno = 22;
    CHECK_ENCODER(EncodeIoctlResponse, EncodedIoctlResponseBufferSize, ioctlResponse);

    Esp32Messaging::GetSockPeerNameResponse peerName = {};
    peerName.AddrLength = THIRD_FIELD_LENGTH;
    peerName.Addr = _fieldData;
    peerName.Result = 0;
    peerName.ResponseErrno = 107;
    CHECK_ENCODER(EncodeGetSockPeerNameResponse, EncodedGetSockPeerNameResponseBufferSize, peerName);
}

TEST_CASE("System, event and Bluetooth encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    FillFieldData();

    Esp32Messaging::SystemConfiguration configuration = {};
    configuration.MaximumMessageQueueLength = 20;
    configuration.MaximumRetryCount = 7;
    configuration.Antenna = 1;
    memset(configuration.BoardMacAddress, 0x34, sizeof(configuration.BoardMacAddress));
    memset(configuration.SoftApMacAddress, 0x35, sizeof(configuration.SoftApMacAddress));
    memset(configuration.BluetoothMacAddress, 0x36, sizeof(configuration.BluetoothMacAddress));
    configuration.DeviceName = const_cast<char *>("MeadowF7");
    configuration.DefaultAccessPoint = const_cast<char *>("BenchNetwork");
    configuration.ResetReason = 3;
    configuration.VersionMajor = 2;
    configuration.VersionMinor = 3;
    configuration.VersionRevision = 4;
    configuration.VersionBuild = 5;
    configuration.BuildDay = 2;
    configuration.BuildMonth = 10;
    configuration.BuildYear = 26;
    configuration.BuildHour = 12;
    configuration.BuildMinute = 34;
    configuration.BuildSecond = 56;
    configuration.BuildHash = 0x41ed21e7;
    configuration.BuildBranchName = const_cast<char *>("Fix/Issue268");
    CHECK_BOUNDED_ENCODER(EncodeSystemConfiguration, EncodedSystemConfigurationBufferSize, configuration);

    Esp32Messaging::GetBatteryChargeLevelResponse battery = {};
    battery.Level = 3300;
    CHECK_ENCODER(EncodeGetBatteryChargeLevelResponse, EncodedGetBatteryChargeLevelResponseBufferSize, battery);

    Esp32Messaging::EventData event = {};
    event.Interface = 1;
    event.Function = 0x01020304;
    event.StatusCode = 0x05060708;
    event.MessageId = 0x090a0b0c;
    CHECK_ENCODER(EncodeEventData, EncodedEventDataBufferSize, event);

    Esp32Messaging::EventDataPayload eventPayload = {};
    eventPayload.MessageId = 0x090a0b0c;
    eventPayload.PayloadLength = FIRST_FIELD_LENGTH;
    eventPayload.Payload = _fieldData;
    CHECK_ENCODER(EncodeEventDataPayload, EncodedEventDataPayloadBufferSize, eventPayload);

    Esp32Messaging::BTDataWriteRequest write = {};
    write.Handle = 0x002a;
    write.DataLength = SECOND_FIELD_LENGTH;
    write.Data = _fieldData;
    CHECK_ENCODER(EncodeBTDataWriteRequest, EncodedBTDataWriteRequestBufferSize, write);

    Esp32Messaging::BTGetHandlesResponse handles = {};
    handles.HandleCount = 3;
    handles.HandlesLength = 6;
    handles.Handles = _fieldData;
    CHECK_ENCODER(EncodeBTGetHandlesResponse, EncodedBTGetHandlesResponseBufferSize, handles);
}

TEST_CASE("File system and core dump encoders write exactly the size their sizers give", CLASS_OF_TESTS)
{
    FillFieldData();

    Esp32Messaging::FileDetails details = {};
    details.Name = const_cast<char *>("hello.txt");
    details.Length = 0x1234;
    CHECK_BOUNDED_ENCODER(EncodeFileDetails, EncodedFileDetailsBufferSize, details);

    Esp32Messaging::FileNameAndContents contents = {};
    contents.Name = const_cast<char *>("hello.txt");
    contents.ContentsLength = THIRD_FIELD_LENGTH;
    contents.Contents = _fieldData;
    CHECK_BOUNDED_ENCODER(EncodeFileNameAndContents, EncodedFileNameAndContentsBufferSize, contents);

    Esp32Messaging::FileNameList list = {};
    list.NumberOfFiles = 2;
    list.FileDetailsLength = SECOND_FIELD_LENGTH;
    list.FileDetails = _fieldData;
    CHECK_ENCODER(EncodeFileNameList, EncodedFileNameListBufferSize, list);

    Esp32Messaging::CoreDumpInformationResponse information = {};
    information.IsValid = 1;
    information.CoreDumpSize = 0x00012345;
    information.PartitionSize = 0x00080000;
    CHECK_ENCODER(EncodeCoreDumpInformation, EncodedCoreDumpInformationBufferSize, information);

    Esp32Messaging::CoreDumpFragmentResponse fragment = {};
    fragment.Size = FIRST_FIELD_LENGTH;
    fragment.Data = _fieldData;
    CHECK_ENCODER(EncodeCoreDumpFragmentResponse, EncodedCoreDumpFragmentResponseBufferSize, fragment);
}
