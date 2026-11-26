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

    struct ShutdownRequest
    {
        int32_t SocketHandle;
        int32_t How;
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

    /**
     * @brief Length of a Bluetooth device address, in bytes.
     */
    constexpr uint32_t BT_ADDRESS_LENGTH = 6;

    /**
     * @brief Address of a Bluetooth device, most significant byte first, as it is usually written.
     */
    struct BTDeviceAddress
    {
        uint8_t Address[BT_ADDRESS_LENGTH];
    };

    /**
     * @brief Payload of PasskeyDisplayEvent, PasskeyRequestEvent and NumericComparisonRequestEvent.
     *
     *  Passkey is the number to show the user; it is 0 in a PasskeyRequestEvent, which asks for one.
     */
    struct BTPairingEventData
    {
        uint8_t Address[BT_ADDRESS_LENGTH];
        uint32_t Passkey;
    };

    /**
     * @brief Payload of PasskeyReply and NumericComparisonReply.
     *
     *  Accept is non-zero to go on with the pairing.  Passkey is the number the user entered, and
     *  is ignored by NumericComparisonReply.
     */
    struct BTPairingReply
    {
        uint8_t Address[BT_ADDRESS_LENGTH];
        uint8_t Accept;
        uint32_t Passkey;
    };

    /**
     * @brief Payload of AuthenticationCompleteEvent.
     *
     *  The event's status code says whether authentication succeeded.  AuthenticationMode holds the
     *  ESP_LE_AUTH_* bits achieved, FailureReason the SMP reason when it failed, and Bonded is non-zero
     *  when the device is now in the bond list.
     */
    struct BTAuthenticationCompleteEventData
    {
        uint8_t Address[BT_ADDRESS_LENGTH];
        uint8_t AddressType;
        uint8_t AuthenticationMode;
        uint8_t FailureReason;
        uint8_t Bonded;
    };

    /**
     * @brief Response to GetBondedDevices.
     *
     *  Devices holds DeviceCount entries of BT_BONDED_DEVICE_ENTRY_LENGTH bytes: the address followed
     *  by its type, 0 for public and 1 for random.
     */
    struct BTBondedDevicesResponse
    {
        uint16_t DeviceCount;
        uint32_t DevicesLength;
        uint8_t *Devices;
    };

    /**
     * @brief Length of one device in a BTBondedDevicesResponse: the address and its type.
     */
    constexpr uint32_t BT_BONDED_DEVICE_ENTRY_LENGTH = BT_ADDRESS_LENGTH + 1;

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

    /**
     *  @brief Number of bytes in an encoded core dump fragment response ahead of the fragment data.
     */
    static constexpr uint32_t CORE_DUMP_FRAGMENT_RESPONSE_HEADER_SIZE = 4;

    static uint32_t CalculateSpiBufferSize(const uint32_t requestedSize);
    static uint32_t EncodedPacketSize(const Message * const message, const bool headerOnly);
    static uint16_t ExtractUInt16(const uint8_t * const buffer);
    static void EncodeUInt16(const uint16_t value, uint8_t * const buffer);
    static uint32_t ExtractUInt32(const uint8_t * const buffer);
    static void EncodeUInt32(const uint32_t value, uint8_t * const buffer);
    static int32_t ExtractInt32(const uint8_t * const buffer);
    static void EncodeInt32(const int32_t value, uint8_t * const buffer);
    static char *ExtractString(const uint8_t * const buffer, uint32_t maxLength = Message::MAXIMUM_PAYLOAD_LENGTH);
    static bool EncodeString(const char * const source, uint8_t *&buffer, uint32_t &remaining);
    static uint32_t StringLength(const char *str);
    static uint8_t CRC8(const uint8_t *data, const uint16_t len);
    static uint32_t CRC32(const uint8_t *data, const uint16_t len);
    static uint32_t ProgressiveCRC32(const uint32_t currentChecksum, const uint8_t byte);
    static bool EncodeMessage(const Message * const message, uint8_t * const buffer, const uint32_t bufferLength, const bool headerOnly);
    static bool PacketIsWellFormed(const uint8_t *buffer, const uint32_t bufferLength, const bool headerOnly);
    static Message *ExtractMessage(const uint8_t *buffer, const uint32_t bufferLength, const bool headerOnly);

    static bool EncodeSystemConfiguration(const Esp32Messaging::SystemConfiguration * const systemConfiguration, uint8_t *buffer, const uint32_t bufferLength);
    static uint32_t EncodedSystemConfigurationBufferSize(const Esp32Messaging::SystemConfiguration * const systemConfiguration);
    static Esp32Messaging::ConfigurationValue *ExtractConfigurationValue(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeConfigurationValue(Esp32Messaging::ConfigurationValue **configurationValue);

    static bool EncodeAccessPointInformation(const Esp32Messaging::AccessPointInformation * const accessPointInformation, uint8_t *buffer, const uint32_t bufferLength);
    static uint32_t EncodedAccessPointInformationBufferSize(const Esp32Messaging::AccessPointInformation * const accessPointInformation);
    static Esp32Messaging::AccessPointInformation *ExtractAccessPointInformation(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeAccessPointInformation(Esp32Messaging::AccessPointInformation **accessPointInformation);

    static Esp32Messaging::DisconnectFromAccessPointRequest *ExtractDisconnectFromAccessPointRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeConnectEventData(const Esp32Messaging::ConnectEventData * const connectEventData, uint8_t *buffer);
    static uint32_t EncodedConnectEventDataBufferSize(const Esp32Messaging::ConnectEventData * const connectEventData);

    static void EncodeDisconnectEventData(const Esp32Messaging::DisconnectEventData * const disconnectEventData, uint8_t *buffer);
    static uint32_t EncodedDisconnectEventDataBufferSize(const Esp32Messaging::DisconnectEventData * const disconnectEventData);

    static void EncodeNodeConnectionChangeEventData(const Esp32Messaging::NodeConnectionChangeEventData * const nodeConnectionChangeEventData, uint8_t *buffer);
    static uint32_t EncodedNodeConnectionChangeEventDataBufferSize(const Esp32Messaging::NodeConnectionChangeEventData * const nodeConnectionChangeEventData);
    static Esp32Messaging::NodeConnectionChangeEventData *ExtractNodeConnectionChangeEventData(uint8_t *);

    static void EncodeAccessPoint(const Esp32Messaging::AccessPoint * const accessPoint, uint8_t *buffer);
    static uint32_t EncodedAccessPointBufferSize(const Esp32Messaging::AccessPoint * const accessPoint);
    static void EncodeAccessPointList(const Esp32Messaging::AccessPointList * const accessPointList, uint8_t *buffer);
    static uint32_t EncodedAccessPointListBufferSize(const Esp32Messaging::AccessPointList * const accessPointList);

    static void EncodeSockAddr(const Esp32Messaging::SockAddr * const sockAddr, uint8_t *buffer);
    static uint32_t EncodedSockAddrBufferSize(const Esp32Messaging::SockAddr * const sockAddr);
    static Esp32Messaging::SockAddr *ExtractSockAddr(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static bool EncodeAddrInfo(const Esp32Messaging::AddrInfo * const addrInfo, uint8_t *buffer, const uint32_t bufferLength);
    static uint32_t EncodedAddrInfoBufferSize(const Esp32Messaging::AddrInfo * const addrInfo);
    static Esp32Messaging::AddrInfo *ExtractAddrInfo(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeAddrInfo(Esp32Messaging::AddrInfo **addrInfo);
    static Esp32Messaging::GetAddrInfoRequest *ExtractGetAddrInfoRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeGetAddrInfoRequest(Esp32Messaging::GetAddrInfoRequest **getAddrInfoRequest);
    static void EncodeGetAddrInfoResponse(const Esp32Messaging::GetAddrInfoResponse * const getAddrInfoResponse, uint8_t *buffer);
    static uint32_t EncodedGetAddrInfoResponseBufferSize(const Esp32Messaging::GetAddrInfoResponse * const getAddrInfoResponse);

    static Esp32Messaging::SocketRequest *ExtractSocketRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeIntegerResponse(const Esp32Messaging::IntegerResponse * const integerResponse, uint8_t *buffer);
    static uint32_t EncodedIntegerResponseBufferSize(const Esp32Messaging::IntegerResponse * const integerResponse);

    static void EncodeIntegerAndErrnoResponse(const Esp32Messaging::IntegerAndErrnoResponse * const integerAndErrnoResponse, uint8_t *buffer);
    static uint32_t EncodedIntegerAndErrnoResponseBufferSize(const Esp32Messaging::IntegerAndErrnoResponse * const integerAndErrnoResponse);

    static Esp32Messaging::ConnectRequest *ExtractConnectRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeConnectRequest(Esp32Messaging::ConnectRequest **connectRequest);

    static Esp32Messaging::FreeAddrInfoRequest *ExtractFreeAddrInfoRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeTimeVal(const Esp32Messaging::TimeVal * const timeVal, uint8_t *buffer);
    static uint32_t EncodedTimeValBufferSize(const Esp32Messaging::TimeVal * const timeVal);

    static Esp32Messaging::TimeVal *ExtractTimeVal(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static Esp32Messaging::SetSockOptRequest *ExtractSetSockOptRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeSetSockOptRequest(Esp32Messaging::SetSockOptRequest **setSockOptRequest);
    static Esp32Messaging::GetSockOptRequest *ExtractGetSockOptRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeGetSockOptResponse(const Esp32Messaging::GetSockOptResponse * const getSockOptResponse, uint8_t *buffer);
    static uint32_t EncodedGetSockOptResponseBufferSize(const Esp32Messaging::GetSockOptResponse * const getSockOptResponse);

    static void EncodeLinger(const Esp32Messaging::Linger * const linger, uint8_t *buffer);
    static uint32_t EncodedLingerBufferSize(const Esp32Messaging::Linger * const linger);

    static Esp32Messaging::WriteRequest *ExtractWriteRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeWriteRequest(Esp32Messaging::WriteRequest **writeRequest);

    static Esp32Messaging::ReadRequest *ExtractReadRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeReadResponse(const Esp32Messaging::ReadResponse * const readResponse, uint8_t *buffer);
    static uint32_t EncodedReadResponseBufferSize(const Esp32Messaging::ReadResponse * const readResponse);

    static Esp32Messaging::CloseRequest *ExtractCloseRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeGetBatteryChargeLevelResponse(const Esp32Messaging::GetBatteryChargeLevelResponse * const getBatteryChargeLevelResponse, uint8_t *buffer);
    static uint32_t EncodedGetBatteryChargeLevelResponseBufferSize(const Esp32Messaging::GetBatteryChargeLevelResponse * const getBatteryChargeLevelResponse);

    static Esp32Messaging::SendRequest *ExtractSendRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeSendRequest(Esp32Messaging::SendRequest **sendRequest);
    static Esp32Messaging::SendToRequest *ExtractSendToRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeSendToRequest(Esp32Messaging::SendToRequest **sendToRequest);

    static Esp32Messaging::RecvFromRequest *ExtractRecvFromRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeRecvFromResponse(const Esp32Messaging::RecvFromResponse * const recvFromResponse, uint8_t *buffer);
    static uint32_t EncodedRecvFromResponseBufferSize(const Esp32Messaging::RecvFromResponse * const recvFromResponse);

    static Esp32Messaging::SendMsgRequest *ExtractSendMsgRequest(const uint8_t *buffer, uint32_t length, bool *malformed);
    static void FreeSendMsgRequest(Esp32Messaging::SendMsgRequest **sendMsgRequest);

    static Esp32Messaging::RecvMsgRequest *ExtractRecvMsgRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static uint32_t EncodedRecvMsgRequestBufferSize(const Esp32Messaging::RecvMsgRequest * const recvMsgRequest);
    static void EncodeRecvMsgResponse(const Esp32Messaging::RecvMsgResponse * const recvMsgResponse, uint8_t *buffer);
    static uint32_t EncodedRecvMsgResponseBufferSize(const Esp32Messaging::RecvMsgResponse * const recvMsgResponse);

    static Esp32Messaging::PollRequest *ExtractPollRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodePollResponse(const Esp32Messaging::PollResponse * const pollResponse, uint8_t *buffer);
    static uint32_t EncodedPollResponseBufferSize(const Esp32Messaging::PollResponse * const pollResponse);

    static void EncodeInterruptPollResponse(const Esp32Messaging::InterruptPollResponse * const interruptPollResponse, uint8_t *buffer);
    static uint32_t EncodedInterruptPollResponseBufferSize(const Esp32Messaging::InterruptPollResponse * const interruptPollResponse);

    static Esp32Messaging::ListenRequest *ExtractListenRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static Esp32Messaging::ShutdownRequest *ExtractShutdownRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static Esp32Messaging::BindRequest *ExtractBindRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeBindRequest(Esp32Messaging::BindRequest **bindRequest);

    static Esp32Messaging::AcceptRequest *ExtractAcceptRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeAcceptResponse(const Esp32Messaging::AcceptResponse * const acceptResponse, uint8_t *buffer);
    static uint32_t EncodedAcceptResponseBufferSize(const Esp32Messaging::AcceptResponse * const acceptResponse);

    static Esp32Messaging::IoctlRequest *ExtractIoctlRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeIoctlResponse(const Esp32Messaging::IoctlResponse * const ioctlResponse, uint8_t *buffer);
    static uint32_t EncodedIoctlResponseBufferSize(const Esp32Messaging::IoctlResponse * const ioctlResponse);

    static Esp32Messaging::GetSockPeerNameRequest *ExtractGetSockPeerNameRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static void EncodeGetSockPeerNameResponse(const Esp32Messaging::GetSockPeerNameResponse * const getSockPeerNameResponse, uint8_t *buffer);
    static uint32_t EncodedGetSockPeerNameResponseBufferSize(const Esp32Messaging::GetSockPeerNameResponse * const getSockPeerNameResponse);

    static void EncodeEventData(const Esp32Messaging::EventData * const eventData, uint8_t *buffer);
    static uint32_t EncodedEventDataBufferSize(const Esp32Messaging::EventData * const eventData);
    static void EncodeEventDataPayload(const Esp32Messaging::EventDataPayload * const eventDataPayload, uint8_t *buffer);
    static uint32_t EncodedEventDataPayloadBufferSize(const Esp32Messaging::EventDataPayload * const eventDataPayload);

    static Esp32Messaging::SetAntennaRequest *ExtractSetAntennaRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);

    static Esp32Messaging::BTStackConfig *ExtractBTStackConfig(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeBTStackConfig(Esp32Messaging::BTStackConfig **bTStackConfig);
    static void EncodeBTDataWriteRequest(const Esp32Messaging::BTDataWriteRequest * const bTDataWriteRequest, uint8_t *buffer);
    static uint32_t EncodedBTDataWriteRequestBufferSize(const Esp32Messaging::BTDataWriteRequest * const bTDataWriteRequest);
    static void EncodeBTGetHandlesResponse(const Esp32Messaging::BTGetHandlesResponse * const bTGetHandlesResponse, uint8_t *buffer);
    static uint32_t EncodedBTGetHandlesResponseBufferSize(const Esp32Messaging::BTGetHandlesResponse * const bTGetHandlesResponse);
    static Esp32Messaging::BTServerDataSet *ExtractBTServerDataSet(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeBTServerDataSet(Esp32Messaging::BTServerDataSet **bTServerDataSet);
    static Esp32Messaging::BTDeviceAddress *ExtractBTDeviceAddress(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static Esp32Messaging::BTPairingReply *ExtractBTPairingReply(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeBTPairingEventData(const Esp32Messaging::BTPairingEventData * const bTPairingEventData, uint8_t *buffer);
    static uint32_t EncodedBTPairingEventDataBufferSize(const Esp32Messaging::BTPairingEventData * const bTPairingEventData);
    static void EncodeBTAuthenticationCompleteEventData(const Esp32Messaging::BTAuthenticationCompleteEventData * const bTAuthenticationCompleteEventData, uint8_t *buffer);
    static uint32_t EncodedBTAuthenticationCompleteEventDataBufferSize(const Esp32Messaging::BTAuthenticationCompleteEventData * const bTAuthenticationCompleteEventData);
    static void EncodeBTBondedDevicesResponse(const Esp32Messaging::BTBondedDevicesResponse * const bTBondedDevicesResponse, uint8_t *buffer);
    static uint32_t EncodedBTBondedDevicesResponseBufferSize(const Esp32Messaging::BTBondedDevicesResponse * const bTBondedDevicesResponse);

    static bool EncodeFileDetails(const Esp32Messaging::FileDetails * const fileDetails, uint8_t *buffer, const uint32_t bufferLength);
    static uint32_t EncodedFileDetailsBufferSize(const Esp32Messaging::FileDetails * const fileDetails);
    static Esp32Messaging::FileDetails *ExtractFileDetails(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeFileDetails(Esp32Messaging::FileDetails **fileDetails);

    static bool EncodeFileNameAndContents(const Esp32Messaging::FileNameAndContents * const fileNameAndContents, uint8_t *buffer, const uint32_t bufferLength);
    static uint32_t EncodedFileNameAndContentsBufferSize(const Esp32Messaging::FileNameAndContents * const fileNameAndContents);
    static Esp32Messaging::FileNameAndContents *ExtractFileNameAndContents(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeFileNameAndContents(Esp32Messaging::FileNameAndContents **fileNameAndContents);

    static void EncodeFileNameList(const Esp32Messaging::FileNameList * const fileNameList, uint8_t *buffer);
    static uint32_t EncodedFileNameListBufferSize(const Esp32Messaging::FileNameList * const fileNameList);

    static Esp32Messaging::LoggingConfiguration *ExtractLoggingConfiguration(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeLoggingConfiguration(Esp32Messaging::LoggingConfiguration **loggingConfiguration);
    static Esp32Messaging::LogMessage *ExtractLogMessage(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void FreeLogMessage(Esp32Messaging::LogMessage **logMessage);

    static void EncodeGotIpEventData(const Esp32Messaging::GotIpEventData * const gotIpEventData, uint8_t *buffer);
    static uint32_t EncodedGotIpEventDataBufferSize(const Esp32Messaging::GotIpEventData * const gotIpEventData);

    static void EncodeCoreDumpInformation(const Esp32Messaging::CoreDumpInformationResponse * const coreDumpInfo, uint8_t *buffer);
    static uint32_t EncodedCoreDumpInformationBufferSize(const Esp32Messaging::CoreDumpInformationResponse * const coreDumpInfo);

    static Esp32Messaging::CoreDumpFragmentRequest *ExtractCoreDumpFragmentRequest(const uint8_t *buffer, uint32_t length, bool *malformed = nullptr);
    static void EncodeCoreDumpFragmentResponse(const Esp32Messaging::CoreDumpFragmentResponse * const fragment, uint8_t *buffer);
    static uint32_t EncodedCoreDumpFragmentResponseBufferSize(const Esp32Messaging::CoreDumpFragmentResponse * const fragment);
};
