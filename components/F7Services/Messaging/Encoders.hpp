/**
 * @file Encoders.hpp
 * @author Mark Stevens
 * @brief Message encoders and decoders.
 * @date 2019-01-18
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "Esp32Messaging.hpp"

namespace Esp32Messaging
{
    struct SystemConfiguration
    {
        uint8_t MaximumMessageQueueLength;
        int32_t MaximumRetryCount;
        uint8_t Antenna;
        uint8_t BoardMacAddress[6];
        uint8_t SoftApMacAddress[6];
        uint8_t BluetoothMacAddress[6];
        char *DeviceName;
        char *DefaultAccessPoint;
        uint8_t ResetReason;
        uint32_t VersionMajor;
        uint32_t VersionMinor;
        uint32_t VersionRevision;
        uint32_t VersionBuild;
        uint8_t BuildDay;
        uint8_t BuildMonth;
        uint8_t BuildYear;
        uint8_t BuildHour;
        uint8_t BuildMinute;
        uint8_t BuildSecond;
        uint32_t BuildHash;
        char *BuildBranchName;
    };

    struct ConfigurationValue
    {
        uint32_t Item;
        uint32_t ValueLength;
        uint8_t *Value;
    };

    struct ErrorEvent
    {
        uint32_t ErrorCode;
        uint8_t Interface;
        uint32_t ErrorDataLength;
        uint8_t *ErrorData;
    };

    struct AccessPointInformation
    {
        char *NetworkName;
        char *Password;
        uint32_t IpAddress;
        uint32_t SubnetMask;
        uint32_t Gateway;
        uint8_t WiFiAuthenticationMode;
        uint8_t Channel;
        uint8_t Hidden;
    };

    struct DisconnectFromAccessPointRequest
    {
        uint8_t TurnOffWiFiInterface;
    };

    struct ConnectEventData
    {
        uint32_t IpAddress;
        uint32_t SubnetMask;
        uint32_t Gateway;
        char Ssid[33];
        uint8_t Bssid[6];
        uint8_t Channel;
        uint8_t AuthenticationMode;
        uint32_t Reason;
    };

    struct NodeConnectionChangeEventData
    {
        uint32_t IpAddress;
        uint8_t MacAddress[6];
    };

    struct DisconnectEventData
    {
        char Ssid[33];
        uint8_t SsidLength;
        uint8_t Bssid[6];
        int8_t Rssi;
        uint8_t Reason;
    };

    struct AccessPoint
    {
        char Ssid[33];
        uint8_t Bssid[6];
        uint8_t PrimaryChannel;
        uint8_t SecondaryChannel;
        int8_t Rssi;
        uint8_t AuthenticationMode;
        uint32_t Protocols;
    };

    struct AccessPointList
    {
        uint32_t NumberOfAccessPoints;
        uint32_t AccessPointsLength;
        uint8_t *AccessPoints;
    };

    struct SockAddr
    {
        uint8_t Family;
        uint16_t Port;
        uint32_t Ip4Address;
        uint32_t FlowInfo;
        uint8_t Ip6Address[16];
        uint32_t ScopeID;
    };

    struct AddrInfo
    {
        uint32_t MyHeapAddress;
        int32_t Flags;
        int32_t Family;
        int32_t SocketType;
        int32_t Protocol;
        uint32_t AddrLen;
        uint32_t AddrLength;
        uint8_t *Addr;
        char *CanonName;
        void *Next;
    };

    struct GetAddrInfoRequest
    {
        char *NodeName;
        char *ServName;
        uint32_t HintsLength;
        uint8_t *Hints;
    };

    struct GetAddrInfoResponse
    {
        int32_t AddrInfoResponseErrno;
        uint32_t ResLength;
        uint8_t *Res;
    };

    struct SocketRequest
    {
        void *AddressInformation;
        int32_t Domain;
        int32_t Type;
        int32_t Protocol;
    };

    struct IntegerResponse
    {
        int32_t Result;
    };

    struct IntegerAndErrnoResponse
    {
        int32_t Result;
        int32_t ResponseErrno;
    };

    struct ConnectRequest
    {
        int32_t SocketHandle;
        uint32_t AddrLength;
        uint8_t *Addr;
    };

    struct FreeAddrInfoRequest
    {
        uint32_t AddrInfoAddress;
    };

    struct TimeVal
    {
        uint32_t TvSec;
        uint32_t TvUsec;
    };

    struct SetSockOptRequest
    {
        int32_t SocketHandle;
        int32_t Level;
        int32_t OptionName;
        uint32_t OptionValueLength;
        uint8_t *OptionValue;
        int32_t OptionLen;
    };

    struct GetSockOptRequest
    {
        int32_t SocketHandle;
        int32_t Level;
        int32_t OptionName;
    };

    struct GetSockOptResponse
    {
        int32_t Result;
        int32_t ResponseErrno;
        uint32_t OptionValueLength;
        uint8_t *OptionValue;
        int32_t OptionLen;
    };

    struct Linger
    {
        int32_t LOnOff;
        int32_t LLinger;
    };

    struct WriteRequest
    {
        int32_t SocketHandle;
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t Count;
    };

    struct ReadRequest
    {
        int32_t SocketHandle;
        int32_t Count;
    };

    struct ReadResponse
    {
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t ReadResponseResult;
        int32_t ReadResponseErrno;
    };

    struct CloseRequest
    {
        int32_t SocketHandle;
    };

    struct GetBatteryChargeLevelResponse
    {
        uint32_t Level;
    };

    struct SendRequest
    {
        int32_t SocketHandle;
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t Length;
        int32_t Flags;
    };

    struct SendToRequest
    {
        int32_t SocketHandle;
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t Length;
        int32_t Flags;
        uint32_t DestinationAddressLength;
        uint8_t *DestinationAddress;
    };

    struct RecvFromRequest
    {
        int32_t SocketHandle;
        int32_t Length;
        int32_t Flags;
        int32_t GetSourceAddress;
    };

    struct RecvFromResponse
    {
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t Result;
        int32_t ResponseErrno;
        uint32_t SourceAddressLength;
        uint8_t *SourceAddress;
        uint32_t SourceAddressLen;
    };

    struct SendMsgRequest
    {
        int32_t SocketHandle;
        int32_t Flags;
        uint32_t NameLength;
        uint8_t *Name;
        uint32_t BufferLength;
        uint8_t *Buffer;
        uint32_t ControlLength;
        uint8_t *Control;
        int32_t MsgFlags;
    };

    struct RecvMsgRequest
    {
        int32_t SocketHandle;
        int32_t Length;
        int32_t Flags;
        int32_t GetSourceAddress;
        uint32_t ControlLength;
    };

    struct RecvMsgResponse
    {
        uint32_t BufferLength;
        uint8_t *Buffer;
        int32_t Result;
        int32_t ResponseErrno;
        uint32_t SourceAddressLength;
        uint8_t *SourceAddress;
        uint32_t SourceAddressLen;
        uint32_t ControlLength;
        uint8_t *Control;
        int32_t MsgFlags;
    };

    struct PollRequest
    {
        int32_t SocketHandle;
        uint16_t Events;
        int32_t Timeout;
        int32_t Setup;
        uint32_t SetupMessageId;
    };

    struct PollResponse
    {
        uint16_t ReturnedEvents;
        int32_t Result;
        int32_t ResponseErrno;
    };

    struct InterruptPollResponse
    {
        int32_t SocketHandle;
        int32_t Result;
        int32_t ResponseErrno;
        uint16_t ReturnedEvents;
        uint32_t SetupMessageId;
    };

    struct ListenRequest
    {
        int32_t SocketHandle;
        int32_t BackLog;
    };

    struct BindRequest
    {
        int32_t SocketHandle;
        uint32_t AddrLength;
        uint8_t *Addr;
    };

    struct AcceptRequest
    {
        int32_t SocketHandle;
    };

    struct AcceptResponse
    {
        uint32_t AddrLength;
        uint8_t *Addr;
        int32_t Result;
        int32_t ResponseErrno;
    };

    struct IoctlRequest
    {
        int32_t Command;
    };

    struct IoctlResponse
    {
        uint32_t AddrLength;
        uint8_t *Addr;
        int32_t Flags;
        int32_t Result;
        int32_t ResponseErrno;
    };

    struct GetSockPeerNameRequest
    {
        int32_t SocketHandle;
    };

    struct GetSockPeerNameResponse
    {
        uint32_t AddrLength;
        uint8_t *Addr;
        int32_t Result;
        int32_t ResponseErrno;
    };

    struct EventData
    {
        uint8_t Interface;
        uint32_t Function;
        uint32_t StatusCode;
        uint32_t MessageId;
    };

    struct EventDataPayload
    {
        uint32_t MessageId;
        uint32_t PayloadLength;
        uint8_t *Payload;
    };

    struct SetAntennaRequest
    {
        uint8_t Antenna;
        uint8_t Persist;
    };

    struct BTStackConfig
    {
        char *Config;
    };

    struct BTDataWriteRequest
    {
        uint16_t Handle;
        uint32_t DataLength;
        uint8_t *Data;
    };

    struct BTGetHandlesResponse
    {
        uint16_t HandleCount;
        uint32_t HandlesLength;
        uint8_t *Handles;
    };

    struct BTServerDataSet
    {
        uint16_t Handle;
        uint32_t SetDataLength;
        uint8_t *SetData;
    };

    struct FileDetails
    {
        char *Name;
        uint16_t Length;
    };

    struct FileNameAndContents
    {
        char *Name;
        uint32_t ContentsLength;
        uint8_t *Contents;
    };

    struct FileNameList
    {
        uint16_t NumberOfFiles;
        uint32_t FileDetailsLength;
        uint8_t *FileDetails;
    };

    /**
     * @brief Logging configuration message data.
     */
    struct LoggingConfiguration
    {
        /**
         * @brief Where should the log information go?
         */
        uint8_t Interface;

        /**
         * @brief Which port should be used if UDP logging is to be used.
         */
        uint16_t UdpPort;

        /**
         * @brief Which components should we log?
         */
        char *ComponentsToLog;
    };

    /**
     * @brief A message to send to the logging system.
     */
    struct LogMessage
    {
        /**
         * @brief Log level for this message.
         */
        uint8_t Level;

        /**
         * @brief Message to send to the logging destination.
         */
        char *Message;
    };

    struct GotIpEventData
    {
        uint32_t DnsAddress;
    };

    /**
     * @brief Core dump information.
     */
    struct CoreDumpInformationResponse
    {
        /**
         * @brief Indicates if the core dump information is valid.
         */
        uint8_t IsValid;

        /**
         * @brief Total size of the core dump.
         */
        uint32_t CoreDumpSize;

        /**
         * @brief Size of the partition containing the core dump.
         */
        uint32_t PartitionSize;
    };

    /**
     * @brief Request for a fragment of the core dump.
     */
    struct CoreDumpFragmentRequest
    {
        /**
         * @brief Offset within the core dump from which to start the fragment.
         */
        uint32_t Offset;

        /**
         * @brief Size of the fragment to retrieve.
         */
        uint32_t Size;
    };

    /**
     * @brief Fragment of the coredump.
     */
    struct CoreDumpFragmentResponse
    {
        /**
         * @brief Size of the fragment.
         */
        uint32_t Size;

        /**
         * @brief Pointer to the fragment data.
         */
        uint8_t *Data;
    };
} // namespace Esp32Messaging

/*
 *      Class holding the encoding methods.
 *
 *      Functionality includes:
 *          - Encoding 16 / 32 bit values into buffers.
 *          - Extracting 16 / 32 bit values from buffer.
 *          - CRC calculations.
 */
class Encoders
{
public:
    /*
     *      Seed for the 32-bit CRC calculation.
     */
    static const uint32_t CRC32_SEED = 0xffffffff;
    static uint32_t CalculateSpiBufferSize(const uint32_t);
    static uint32_t EncodedPacketSize(const Message * const, const bool);
    static uint16_t ExtractUInt16(const uint8_t * const);
    static void EncodeUInt16(const uint16_t, uint8_t * const);
    static uint32_t ExtractUInt32(const uint8_t * const);
    static void EncodeUInt32(const uint32_t, uint8_t * const);
    static uint32_t ExtractInt32(const uint8_t * const);
    static void EncodeInt32(const int32_t, uint8_t * const);
    static char *ExtractString(const uint8_t * const, const uint32_t maxLength = Message::MAXIMUM_PAYLOAD_LENGTH);
    static void EncodeString(char *, uint8_t *);
    static uint32_t StringLength(const char *);
    static uint8_t CRC8(const uint8_t *, const uint16_t);
    static uint32_t CRC32(const uint8_t *, const uint16_t);
    static uint32_t ProgressiveCRC32(const uint32_t, const uint8_t);
    static void EncodeMessage(const Message * const, uint8_t * const, const bool);
    static Message *ExtractMessage(uint8_t *, const uint32_t, const bool);

    static void EncodeSystemConfiguration(const Esp32Messaging::SystemConfiguration * const, uint8_t *);
    static int EncodedSystemConfigurationBufferSize(const Esp32Messaging::SystemConfiguration * const);
    static Esp32Messaging::ConfigurationValue *ExtractConfigurationValue(uint8_t *);
    static void FreeConfigurationValue(Esp32Messaging::ConfigurationValue **);

    static void EncodeAccessPointInformation(const Esp32Messaging::AccessPointInformation * const, uint8_t *);
    static int EncodedAccessPointInformationBufferSize(const Esp32Messaging::AccessPointInformation * const);
    static Esp32Messaging::AccessPointInformation *ExtractAccessPointInformation(uint8_t *);
    static void FreeAccessPointInformation(Esp32Messaging::AccessPointInformation **);

    static Esp32Messaging::DisconnectFromAccessPointRequest *ExtractDisconnectFromAccessPointRequest(uint8_t *);

    static void EncodeConnectEventData(const Esp32Messaging::ConnectEventData * const, uint8_t *);
    static int EncodedConnectEventDataBufferSize(const Esp32Messaging::ConnectEventData * const);

    static void EncodeDisconnectEventData(const Esp32Messaging::DisconnectEventData * const, uint8_t *);
    static int EncodeDisconnectEventDataBufferSize(const Esp32Messaging::DisconnectEventData * const);

    static void EncodeNodeConnectionChangeEventData(const Esp32Messaging::NodeConnectionChangeEventData * const, uint8_t *);
    static int EncodedNodeConnectionChangeEventDataBufferSize(const Esp32Messaging::NodeConnectionChangeEventData * const);
    static Esp32Messaging::NodeConnectionChangeEventData *ExtractNodeConnectionChangeEventData(uint8_t *);

    static void EncodeAccessPoint(const Esp32Messaging::AccessPoint * const, uint8_t *);
    static int EncodedAccessPointBufferSize(const Esp32Messaging::AccessPoint * const);
    static void EncodeAccessPointList(const Esp32Messaging::AccessPointList * const, uint8_t *);
    static int EncodedAccessPointListBufferSize(const Esp32Messaging::AccessPointList * const);

    static void EncodeSockAddr(const Esp32Messaging::SockAddr * const, uint8_t *);
    static int EncodedSockAddrBufferSize(const Esp32Messaging::SockAddr * const);
    static Esp32Messaging::SockAddr *ExtractSockAddr(uint8_t *);

    static void EncodeAddrInfo(const Esp32Messaging::AddrInfo * const, uint8_t *);
    static int EncodedAddrInfoBufferSize(const Esp32Messaging::AddrInfo * const);
    static Esp32Messaging::AddrInfo *ExtractAddrInfo(uint8_t *);
    static void FreeAddrInfo(Esp32Messaging::AddrInfo **);
    static Esp32Messaging::GetAddrInfoRequest *ExtractGetAddrInfoRequest(uint8_t *);
    static void FreeGetAddrInfoRequest(Esp32Messaging::GetAddrInfoRequest **);
    static void EncodeGetAddrInfoResponse(const Esp32Messaging::GetAddrInfoResponse * const, uint8_t *);
    static int EncodedGetAddrInfoResponseBufferSize(const Esp32Messaging::GetAddrInfoResponse * const);

    static Esp32Messaging::SocketRequest *ExtractSocketRequest(uint8_t *);

    static void EncodeIntegerResponse(const Esp32Messaging::IntegerResponse * const, uint8_t *);
    static int EncodedIntegerResponseBufferSize(const Esp32Messaging::IntegerResponse * const);

    static void EncodeIntegerAndErrnoResponse(const Esp32Messaging::IntegerAndErrnoResponse * const, uint8_t *);
    static int EncodedIntegerAndErrnoResponseBufferSize(const Esp32Messaging::IntegerAndErrnoResponse * const);

    static Esp32Messaging::ConnectRequest *ExtractConnectRequest(uint8_t *);
    static void FreeConnectRequest(Esp32Messaging::ConnectRequest **);

    static Esp32Messaging::FreeAddrInfoRequest *ExtractFreeAddrInfoRequest(uint8_t *);

    static void EncodeTimeVal(const Esp32Messaging::TimeVal * const, uint8_t *);
    static int EncodedTimeValBufferSize(const Esp32Messaging::TimeVal * const);

    static Esp32Messaging::TimeVal *ExtractTimeVal(uint8_t *);

    static Esp32Messaging::SetSockOptRequest *ExtractSetSockOptRequest(uint8_t *);
    static void FreeSetSockOptRequest(Esp32Messaging::SetSockOptRequest **);
    static Esp32Messaging::GetSockOptRequest *ExtractGetSockOptRequest(uint8_t *);
    static void EncodeGetSockOptResponse(const Esp32Messaging::GetSockOptResponse * const, uint8_t *);
    static int EncodedGetSockOptResponseBufferSize(const Esp32Messaging::GetSockOptResponse * const);

    static void EncodeLinger(const Esp32Messaging::Linger * const, uint8_t *);
    static int EncodedLingerBufferSize(const Esp32Messaging::Linger * const);

    static Esp32Messaging::WriteRequest *ExtractWriteRequest(uint8_t *);
    static void FreeWriteRequest(Esp32Messaging::WriteRequest **);

    static Esp32Messaging::ReadRequest *ExtractReadRequest(uint8_t *);
    static void EncodeReadResponse(const Esp32Messaging::ReadResponse * const, uint8_t *);
    static int EncodedReadResponseBufferSize(const Esp32Messaging::ReadResponse * const);

    static Esp32Messaging::CloseRequest *ExtractCloseRequest(uint8_t *);

    static void EncodeGetBatteryChargeLevelResponse(const Esp32Messaging::GetBatteryChargeLevelResponse * const, uint8_t *);
    static int EncodedGetBatteryChargeLevelResponseBufferSize(const Esp32Messaging::GetBatteryChargeLevelResponse * const);

    static Esp32Messaging::SendRequest *ExtractSendRequest(uint8_t *);
    static void FreeSendRequest(Esp32Messaging::SendRequest **);
    static Esp32Messaging::SendToRequest *ExtractSendToRequest(uint8_t *);
    static void FreeSendToRequest(Esp32Messaging::SendToRequest **);

    static Esp32Messaging::RecvFromRequest *ExtractRecvFromRequest(uint8_t *);
    static void EncodeRecvFromResponse(const Esp32Messaging::RecvFromResponse * const, uint8_t *);
    static int EncodedRecvFromResponseBufferSize(const Esp32Messaging::RecvFromResponse * const);

    static Esp32Messaging::SendMsgRequest *ExtractSendMsgRequest(uint8_t *);
    static void FreeSendMsgRequest(Esp32Messaging::SendMsgRequest **);

    static Esp32Messaging::RecvMsgRequest *ExtractRecvMsgRequest(uint8_t *);
    static int EncodedRecvMsgRequestBufferSize(const Esp32Messaging::RecvMsgRequest * const);
    static void EncodeRecvMsgResponse(const Esp32Messaging::RecvMsgResponse * const, uint8_t *);
    static int EncodedRecvMsgResponseBufferSize(const Esp32Messaging::RecvMsgResponse * const);

    static Esp32Messaging::PollRequest *ExtractPollRequest(uint8_t *);
    static void EncodePollResponse(const Esp32Messaging::PollResponse * const, uint8_t *);
    static int EncodedPollResponseBufferSize(const Esp32Messaging::PollResponse * const);

    static void EncodeInterruptPollResponse(const Esp32Messaging::InterruptPollResponse * const, uint8_t *);
    static int EncodedInterruptPollResponseBufferSize(const Esp32Messaging::InterruptPollResponse * const);

    static Esp32Messaging::ListenRequest *ExtractListenRequest(uint8_t *);

    static Esp32Messaging::BindRequest *ExtractBindRequest(uint8_t *);
    static void FreeBindRequest(Esp32Messaging::BindRequest **);

    static Esp32Messaging::AcceptRequest *ExtractAcceptRequest(uint8_t *);

    static void EncodeAcceptResponse(const Esp32Messaging::AcceptResponse * const, uint8_t *);
    static int EncodedAcceptResponseBufferSize(const Esp32Messaging::AcceptResponse * const);

    static void EncodeIoctlRequest(const Esp32Messaging::IoctlRequest * const, uint8_t *);
    static int EncodedIoctlRequestBufferSize(const Esp32Messaging::IoctlRequest * const);

    static Esp32Messaging::IoctlRequest *ExtractIoctlRequest(uint8_t *);
    static void EncodeIoctlResponse(const Esp32Messaging::IoctlResponse * const, uint8_t *);
    static int EncodedIoctlResponseBufferSize(const Esp32Messaging::IoctlResponse * const);

    static Esp32Messaging::GetSockPeerNameRequest *ExtractGetSockPeerNameRequest(uint8_t *);

    static void EncodeGetSockPeerNameResponse(const Esp32Messaging::GetSockPeerNameResponse * const, uint8_t *);
    static int EncodedGetSockPeerNameResponseBufferSize(const Esp32Messaging::GetSockPeerNameResponse * const);

    static void EncodeEventData(const Esp32Messaging::EventData * const, uint8_t *);
    static int EncodedEventDataBufferSize(const Esp32Messaging::EventData * const);
    static void EncodeEventDataPayload(const Esp32Messaging::EventDataPayload * const, uint8_t *);
    static int EncodedEventDataPayloadBufferSize(const Esp32Messaging::EventDataPayload * const);

    static Esp32Messaging::SetAntennaRequest *ExtractSetAntennaRequest(uint8_t *);

    static Esp32Messaging::BTStackConfig *ExtractBTStackConfig(uint8_t *);
    static void FreeBTStackConfig(Esp32Messaging::BTStackConfig **);
    static void EncodeBTDataWriteRequest(const Esp32Messaging::BTDataWriteRequest * const, uint8_t *);
    static int EncodedBTDataWriteRequestBufferSize(const Esp32Messaging::BTDataWriteRequest * const);
    static void EncodeBTGetHandlesResponse(const Esp32Messaging::BTGetHandlesResponse * const, uint8_t *);
    static int EncodedBTGetHandlesResponseBufferSize(const Esp32Messaging::BTGetHandlesResponse * const);
    static Esp32Messaging::BTServerDataSet *ExtractBTServerDataSet(uint8_t *);
    static void FreeBTServerDataSet(Esp32Messaging::BTServerDataSet **);

    static void EncodeFileDetails(const Esp32Messaging::FileDetails * const, uint8_t *);
    static int EncodedFileDetailsBufferSize(const Esp32Messaging::FileDetails * const);
    static Esp32Messaging::FileDetails *ExtractFileDetails(uint8_t *);
    static void FreeFileDetails(Esp32Messaging::FileDetails **);

    static void EncodeFileNameAndContents(const Esp32Messaging::FileNameAndContents * const, uint8_t *);
    static int EncodedFileNameAndContentsBufferSize(const Esp32Messaging::FileNameAndContents * const);
    static Esp32Messaging::FileNameAndContents *ExtractFileNameAndContents(uint8_t *);
    static void FreeFileNameAndContents(Esp32Messaging::FileNameAndContents **);

    static void EncodeFileNameList(const Esp32Messaging::FileNameList * const, uint8_t *);
    static int EncodedFileNameListBufferSize(const Esp32Messaging::FileNameList * const);

    static Esp32Messaging::LoggingConfiguration *ExtractLoggingConfiguration(uint8_t *);
    static void FreeLoggingConfiguration(Esp32Messaging::LoggingConfiguration **);
    static Esp32Messaging::LogMessage *ExtractLogMessage(uint8_t *);
    static void FreeLogMessage(Esp32Messaging::LogMessage **);

    static void EncodeGotIpEventData(const Esp32Messaging::GotIpEventData * const, uint8_t *);
    static int EncodedGotIpEventDataBufferSize(const Esp32Messaging::GotIpEventData * const);

    static void EncodeCoreDumpInformation(const Esp32Messaging::CoreDumpInformationResponse * const, uint8_t *);
    static int EncodedCoreDumpInformationBufferSize(const Esp32Messaging::CoreDumpInformationResponse * const);

    static Esp32Messaging::CoreDumpFragmentRequest *ExtractCoreDumpFragmentRequest(uint8_t *);
    static void EncodeCoreDumpFragmentResponse(const Esp32Messaging::CoreDumpFragmentResponse * const, uint8_t *);
    static int EncodedCoreDumpFragmentResponseBufferSize(const Esp32Messaging::CoreDumpFragmentResponse * const);
};
