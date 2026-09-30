/**
 * @file TestEncoders.cpp
 * @author Mark Stevens
 * @brief Encoding and extraction methods that mirror the Encoders class.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "unity.h"

#include "Encoders.hpp"
#include "Utils.hpp"

#include "TestEncoders.hpp"

namespace TestEncoders
{
    /**
     * @brief Extract the IntegerResponse object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded IntegerResponse object.
     *
     * @return Pointer to an IntegerResponse object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::IntegerResponse *ExtractIntegerResponse(const uint8_t *buffer)
    {
        Esp32Messaging::IntegerResponse *integerResponse = static_cast<Esp32Messaging::IntegerResponse *>(pvPortMalloc(sizeof(Esp32Messaging::IntegerResponse)));

        if (integerResponse == nullptr)
        {
            return (nullptr);
        }

        integerResponse->Result = Encoders::ExtractInt32(buffer);

        return (integerResponse);
    }

    /**
     * @brief Extract the ConnectEventData object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded ConnectEventData
     *      object.
     *
     * @return Pointer to a ConnectEventData object, nullptr if it cannot be allocated.
     */
    Esp32Messaging::ConnectEventData *ExtractConnectEventData(const uint8_t *buffer)
    {
        Esp32Messaging::ConnectEventData *connectEventData = static_cast<Esp32Messaging::ConnectEventData *>(pvPortMalloc(sizeof(Esp32Messaging::ConnectEventData)));

        if (connectEventData == nullptr)
        {
            return (nullptr);
        }

        connectEventData->IpAddress = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        connectEventData->SubnetMask = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        connectEventData->Gateway = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        memcpy(connectEventData->Ssid, buffer, 33);
        buffer += 33;
        memcpy(connectEventData->Bssid, buffer, 6);
        buffer += 6;
        connectEventData->Channel = *buffer;
        buffer += 1;
        connectEventData->AuthenticationMode = *buffer;
        buffer += 1;
        connectEventData->Reason = Encoders::ExtractUInt32(buffer);

        return (connectEventData);
    }

    /**
     * @brief Extract the AccessPointList object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded AccessPointList object.
     *
     * @return Pointer to an AccessPointList object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::AccessPointList *ExtractAccessPointList(const uint8_t *buffer)
    {
        Esp32Messaging::AccessPointList *accessPointList = static_cast<Esp32Messaging::AccessPointList *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointList)));

        if (accessPointList == nullptr)
        {
            return (nullptr);
        }

        accessPointList->NumberOfAccessPoints = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        accessPointList->AccessPointsLength = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        if (accessPointList->AccessPointsLength > 0)
        {
            accessPointList->AccessPoints = static_cast<uint8_t *>(pvPortMalloc(accessPointList->AccessPointsLength));
            if (accessPointList->AccessPoints == nullptr)
            {
                Utils::DeleteAndNullifyPointer(accessPointList);
                return (nullptr);
            }
            memcpy(accessPointList->AccessPoints, buffer, accessPointList->AccessPointsLength);
        }
        else
        {
            accessPointList->AccessPoints = nullptr;
        }

        return (accessPointList);
    }

    /**
     * @brief Release an AccessPointList object and the access points it holds.
     *
     * @param accessPointList Pointer to the pointer to the object to be released.
     */
    void FreeAccessPointList(Esp32Messaging::AccessPointList **accessPointList)
    {
        if ((accessPointList == nullptr) || (*accessPointList == nullptr))
        {
            return;
        }

        if ((*accessPointList)->AccessPoints != nullptr)
        {
            Utils::DeleteAndNullifyPointer((*accessPointList)->AccessPoints);
        }
        Utils::DeleteAndNullifyPointer(*accessPointList);
    }

    /**
     * @brief Extract the AccessPoint object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded AccessPoint object.
     *
     * @return Pointer to an AccessPoint object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::AccessPoint *ExtractAccessPoint(const uint8_t *buffer)
    {
        Esp32Messaging::AccessPoint *accessPoint = static_cast<Esp32Messaging::AccessPoint *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPoint)));

        if (accessPoint == nullptr)
        {
            return (nullptr);
        }

        memcpy(accessPoint->Ssid, buffer, sizeof(accessPoint->Ssid));
        buffer += sizeof(accessPoint->Ssid);
        memcpy(accessPoint->Bssid, buffer, sizeof(accessPoint->Bssid));
        buffer += sizeof(accessPoint->Bssid);
        accessPoint->PrimaryChannel = *buffer;
        buffer += 1;
        accessPoint->SecondaryChannel = *buffer;
        buffer += 1;
        accessPoint->Rssi = static_cast<int8_t>(*buffer);
        buffer += 1;
        accessPoint->AuthenticationMode = *buffer;
        buffer += 1;
        accessPoint->Protocols = Encoders::ExtractUInt32(buffer);

        return (accessPoint);
    }

    /**
     * @brief Extract the FileNameList object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded FileNameList object.
     *
     * @return Pointer to a FileNameList object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::FileNameList *ExtractFileNameList(const uint8_t *buffer)
    {
        Esp32Messaging::FileNameList *fileNameList = static_cast<Esp32Messaging::FileNameList *>(pvPortMalloc(sizeof(Esp32Messaging::FileNameList)));

        if (fileNameList == nullptr)
        {
            return (nullptr);
        }

        fileNameList->NumberOfFiles = Encoders::ExtractUInt16(buffer);
        buffer += 2;
        fileNameList->FileDetailsLength = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        if (fileNameList->FileDetailsLength > 0)
        {
            fileNameList->FileDetails = static_cast<uint8_t *>(pvPortMalloc(fileNameList->FileDetailsLength));
            if (fileNameList->FileDetails == nullptr)
            {
                Utils::DeleteAndNullifyPointer(fileNameList);
                return (nullptr);
            }
            memcpy(fileNameList->FileDetails, buffer, fileNameList->FileDetailsLength);
        }
        else
        {
            fileNameList->FileDetails = nullptr;
        }

        return (fileNameList);
    }

    /**
     * @brief Release a FileNameList object and the file details it holds.
     *
     * @param fileNameList Pointer to the pointer to the object to be released.
     */
    void FreeFileNameList(Esp32Messaging::FileNameList **fileNameList)
    {
        if ((fileNameList == nullptr) || (*fileNameList == nullptr))
        {
            return;
        }

        if ((*fileNameList)->FileDetails != nullptr)
        {
            Utils::DeleteAndNullifyPointer((*fileNameList)->FileDetails);
        }
        Utils::DeleteAndNullifyPointer(*fileNameList);
    }

    /**
     * @brief Extract the CoreDumpInformationResponse object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded object, nullptr for an
     *               empty response.
     *
     * @return Pointer to a CoreDumpInformationResponse object, nullptr if the object cannot
     *         be allocated.
     */
    Esp32Messaging::CoreDumpInformationResponse *ExtractCoreDumpInformation(const uint8_t *buffer)
    {
        Esp32Messaging::CoreDumpInformationResponse *information = static_cast<Esp32Messaging::CoreDumpInformationResponse *>(pvPortMalloc(sizeof(Esp32Messaging::CoreDumpInformationResponse)));

        if (information == nullptr)
        {
            return (nullptr);
        }

        if (buffer == nullptr)
        {
            information->IsValid = 0;
            information->CoreDumpSize = 0;
            information->PartitionSize = 0;
            return (information);
        }

        information->IsValid = *buffer;
        buffer++;
        information->CoreDumpSize = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        information->PartitionSize = Encoders::ExtractUInt32(buffer);

        return (information);
    }

    /**
     * @brief Calculate the memory required to hold an encoded CoreDumpFragmentRequest object.
     *
     * @param request Pointer to the CoreDumpFragmentRequest object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedCoreDumpFragmentRequestBufferSize(const Esp32Messaging::CoreDumpFragmentRequest * const request)
    {
        (void) request;

        return (sizeof(uint32_t) + sizeof(uint32_t));
    }

    /**
     * @brief Encode a CoreDumpFragmentRequest object into a newly allocated buffer.
     *
     * @param request Pointer to the CoreDumpFragmentRequest object to be encoded.
     *
     * @return Pointer to the encoded object, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeCoreDumpFragmentRequest(const Esp32Messaging::CoreDumpFragmentRequest * const request)
    {
        if (request == nullptr)
        {
            return (nullptr);
        }

        uint8_t *fragment = static_cast<uint8_t *>(pvPortMalloc(EncodedCoreDumpFragmentRequestBufferSize(request)));
        if (fragment != nullptr)
        {
            Encoders::EncodeUInt32(request->Offset, fragment);
            Encoders::EncodeUInt32(request->Size, fragment + 4);
        }

        return (fragment);
    }

    /**
     * @brief Extract the CoreDumpFragmentResponse object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded object.
     *
     * @return Pointer to a CoreDumpFragmentResponse object, nullptr if the object cannot be
     *         allocated.
     */
    Esp32Messaging::CoreDumpFragmentResponse *ExtractCoreDumpFragmentResponse(const uint8_t *buffer)
    {
        if (buffer == nullptr)
        {
            return (nullptr);
        }

        Esp32Messaging::CoreDumpFragmentResponse *fragmentResponse = static_cast<Esp32Messaging::CoreDumpFragmentResponse *>(pvPortMalloc(sizeof(Esp32Messaging::CoreDumpFragmentResponse)));
        if (fragmentResponse == nullptr)
        {
            return (nullptr);
        }

        fragmentResponse->Size = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        if (fragmentResponse->Size > 0)
        {
            fragmentResponse->Data = static_cast<uint8_t *>(pvPortMalloc(fragmentResponse->Size));
            if (fragmentResponse->Data == nullptr)
            {
                Utils::DeleteAndNullifyPointer(fragmentResponse);
                return (nullptr);
            }
            memcpy(fragmentResponse->Data, buffer, fragmentResponse->Size);
        }
        else
        {
            fragmentResponse->Data = nullptr;
        }

        return (fragmentResponse);
    }

    /**
     * @brief Release a CoreDumpFragmentResponse object and the data it holds.
     *
     * @param fragmentResponse Pointer to the pointer to the object to be released.
     */
    void FreeCoreDumpFragmentResponse(Esp32Messaging::CoreDumpFragmentResponse **fragmentResponse)
    {
        if ((fragmentResponse == nullptr) || (*fragmentResponse == nullptr))
        {
            return;
        }

        if ((*fragmentResponse)->Data != nullptr)
        {
            Utils::DeleteAndNullifyPointer((*fragmentResponse)->Data);
        }
        Utils::DeleteAndNullifyPointer(*fragmentResponse);
    }

    /**
     * @brief Calculate the memory required to hold an encoded BTStackConfig object.
     *
     * @param bTStackConfig Pointer to the BTStackConfig object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedBTStackConfigBufferSize(const Esp32Messaging::BTStackConfig * const bTStackConfig)
    {
        return (Encoders::StringLength(bTStackConfig->Config) + 1);
    }

    /**
     * @brief Encode a BTStackConfig object into the given buffer.
     *
     * @param bTStackConfig Pointer to the BTStackConfig object to be encoded.
     * @param buffer Pointer to the block of memory to hold the encoded object.
     */
    void EncodeBTStackConfig(const Esp32Messaging::BTStackConfig * const bTStackConfig, uint8_t *buffer)
    {
        uint32_t remaining = EncodedBTStackConfigBufferSize(bTStackConfig);
        TEST_ASSERT_TRUE(Encoders::EncodeString(bTStackConfig->Config, buffer, remaining));
    }

    /**
     * @brief Calculate the memory required to hold an encoded BTServerDataSet object.
     *
     * @param bTServerDataSet Pointer to the BTServerDataSet object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedBTServerDataSetBufferSize(const Esp32Messaging::BTServerDataSet * const bTServerDataSet)
    {
        return (sizeof(uint16_t) + sizeof(uint32_t) + bTServerDataSet->SetDataLength);
    }

    /**
     * @brief Encode a BTServerDataSet object into the given buffer.
     *
     * @param bTServerDataSet Pointer to the BTServerDataSet object to be encoded.
     * @param buffer Pointer to the block of memory to hold the encoded object.
     */
    void EncodeBTServerDataSet(const Esp32Messaging::BTServerDataSet * const bTServerDataSet, uint8_t *buffer)
    {
        Encoders::EncodeUInt16(bTServerDataSet->Handle, buffer);
        buffer += 2;
        Encoders::EncodeUInt32(bTServerDataSet->SetDataLength, buffer);
        buffer += 4;
        if (bTServerDataSet->SetDataLength > 0)
        {
            memcpy(buffer, bTServerDataSet->SetData, bTServerDataSet->SetDataLength);
        }
    }

    /**
     * @brief Encode a socket request into a newly allocated buffer.
     *
     * @param domain Address family for the socket.
     * @param type Type of socket required.
     * @param protocol Protocol the socket should use.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSocketRequest(const int32_t domain, const int32_t type, const int32_t protocol)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_SOCKET_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, ENCODED_SOCKET_REQUEST_SIZE);

        Encoders::EncodeUInt32(0, buffer); // AddressInformation is not used here.
        Encoders::EncodeInt32(domain, buffer + 4);
        Encoders::EncodeInt32(type, buffer + 8);
        Encoders::EncodeInt32(protocol, buffer + 12);

        return (buffer);
    }

    /**
     * @brief Encode a connect request for an IPv4 address into a newly allocated buffer.
     *
     * @param socketHandle Socket to connect.
     * @param address Dotted quad address of the server.
     * @param port Port to connect to, in host byte order.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeConnectRequest(const int32_t socketHandle, const char *address, const uint16_t port)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_CONNECT_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, ENCODED_CONNECT_REQUEST_SIZE);

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeUInt32(ENCODED_SOCK_ADDR_SIZE, buffer + 4);

        uint8_t *sockAddr = buffer + 8;
        sockAddr[0] = NUTTX_AF_INET;
        Encoders::EncodeUInt16(htons(port), sockAddr + 1);
        Encoders::EncodeUInt32(inet_addr(address), sockAddr + 3);

        return (buffer);
    }

    /**
     * @brief Encode a send request into a newly allocated buffer.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendRequest(const int32_t socketHandle, const char *data, const uint32_t length, uint32_t *encodedLength)
    {
        *encodedLength = 4 + 4 + length + 4 + 4;
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, *encodedLength);

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeUInt32(length, buffer + 4);
        memcpy(buffer + 8, data, length);
        Encoders::EncodeInt32(static_cast<int32_t>(length), buffer + 8 + length);
        Encoders::EncodeInt32(0, buffer + 12 + length); // No flags.

        return (buffer);
    }

    /**
     * @brief Encode a send request whose wire Length field disagrees with the size of the
     *        buffer it actually carries.  See TestEncoders.hpp for the full explanation.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be copied into the buffer, bufferLength bytes of it.
     * @param bufferLength Number of bytes of data copied in, and the wire BufferLength.
     * @param declaredLength Value encoded into the wire Length field.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendRequestWithDeclaredLength(const int32_t socketHandle, const char *data, const uint32_t bufferLength, const int32_t declaredLength, uint32_t *encodedLength)
    {
        *encodedLength = 4 + 4 + bufferLength + 4 + 4;
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, *encodedLength);

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeUInt32(bufferLength, buffer + 4);
        memcpy(buffer + 8, data, bufferLength);
        Encoders::EncodeInt32(declaredLength, buffer + 8 + bufferLength);
        Encoders::EncodeInt32(0, buffer + 12 + bufferLength); // No flags.

        return (buffer);
    }

    /**
     * @brief Encode a sendto request into a newly allocated buffer.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param destinationAddress Encoded SockAddr bytes for the destination, or nullptr.
     * @param destinationAddressLength Length of destinationAddress, 0 for no destination.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendToRequest(const int32_t socketHandle, const char *data, const uint32_t length, const uint8_t *destinationAddress, const uint32_t destinationAddressLength, uint32_t *encodedLength)
    {
        *encodedLength = 4 + 4 + length + 4 + 4 + 4 + destinationAddressLength;
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, *encodedLength);

        uint8_t *p = buffer;
        Encoders::EncodeInt32(socketHandle, p);
        p += 4;
        Encoders::EncodeUInt32(length, p);
        p += 4;
        if (length > 0)
        {
            memcpy(p, data, length);
            p += length;
        }
        Encoders::EncodeInt32(static_cast<int32_t>(length), p); // Length
        p += 4;
        Encoders::EncodeInt32(0, p);                            // Flags
        p += 4;
        Encoders::EncodeUInt32(destinationAddressLength, p);
        p += 4;
        if (destinationAddressLength > 0)
        {
            memcpy(p, destinationAddress, destinationAddressLength);
        }

        return (buffer);
    }

    /**
     * @brief Encode a sendto request whose wire Length field disagrees with the size of
     *        the buffer it actually carries.  See TestEncoders.hpp for the full
     *        explanation.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be copied into the buffer, bufferLength bytes of it.
     * @param bufferLength Number of bytes of data copied in, and the wire BufferLength.
     * @param declaredLength Value encoded into the wire Length field.
     * @param destinationAddress Encoded SockAddr bytes for the destination, or nullptr.
     * @param destinationAddressLength Length of destinationAddress, 0 for no destination.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendToRequestWithDeclaredLength(const int32_t socketHandle, const char *data, const uint32_t bufferLength, const int32_t declaredLength, const uint8_t *destinationAddress, const uint32_t destinationAddressLength, uint32_t *encodedLength)
    {
        *encodedLength = 4 + 4 + bufferLength + 4 + 4 + 4 + destinationAddressLength;
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, *encodedLength);

        uint8_t *p = buffer;
        Encoders::EncodeInt32(socketHandle, p);
        p += 4;
        Encoders::EncodeUInt32(bufferLength, p);
        p += 4;
        if (bufferLength > 0)
        {
            memcpy(p, data, bufferLength);
            p += bufferLength;
        }
        Encoders::EncodeInt32(declaredLength, p); // Length
        p += 4;
        Encoders::EncodeInt32(0, p);              // Flags
        p += 4;
        Encoders::EncodeUInt32(destinationAddressLength, p);
        p += 4;
        if (destinationAddressLength > 0)
        {
            memcpy(p, destinationAddress, destinationAddressLength);
        }

        return (buffer);
    }

    /**
     * @brief Encode a sendmsg request carrying a single iovec into a newly allocated buffer.
     *
     * @param socketHandle Socket to send on.
     * @param name Raw NuttX sockaddr bytes for the destination name, or nullptr.
     * @param nameLength Length of name, 0 for none.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendMsgRequest(const int32_t socketHandle, const uint8_t *name, const uint32_t nameLength, const char *data, const uint32_t length, uint32_t *encodedLength)
    {
        *encodedLength = 4 + 4 + 4 + 4 + nameLength + 4 + 4 + length + 4 + 4;
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, *encodedLength);

        uint8_t *p = buffer;
        Encoders::EncodeInt32(socketHandle, p);
        p += 4;
        Encoders::EncodeInt32(0, p);  // Flags
        p += 4;
        Encoders::EncodeUInt32(0, p); // MessageHeaderLength; skipped on extraction.
        p += 4;
        Encoders::EncodeUInt32(nameLength, p);
        p += 4;
        if (nameLength > 0)
        {
            memcpy(p, name, nameLength);
            p += nameLength;
        }
        Encoders::EncodeUInt32(1, p);      // One iovec.
        p += 4;
        Encoders::EncodeUInt32(length, p); // iovec[0] length.
        p += 4;
        if (length > 0)
        {
            memcpy(p, data, length);
            p += length;
        }
        Encoders::EncodeUInt32(0, p); // ControlLength.
        p += 4;
        Encoders::EncodeInt32(0, p);  // MsgFlags.

        return (buffer);
    }

    /**
     * @brief Encode a recvfrom request into a newly allocated buffer.
     *
     * @param socketHandle Socket to receive from.
     * @param length Largest number of bytes wanted.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeRecvFromRequest(const int32_t socketHandle, const int32_t length)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_RECV_FROM_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, ENCODED_RECV_FROM_REQUEST_SIZE);

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeInt32(length, buffer + 4);
        Encoders::EncodeInt32(0, buffer + 8);  // No flags.
        Encoders::EncodeInt32(1, buffer + 12); // Ask for the source address.

        return (buffer);
    }

    /**
     * @brief Encode a recvmsg request into a newly allocated buffer.
     *
     * @param socketHandle Socket to receive from.
     * @param length Largest number of bytes wanted.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeRecvMsgRequest(const int32_t socketHandle, const int32_t length)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_RECV_MSG_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }
        memset(buffer, 0, ENCODED_RECV_MSG_REQUEST_SIZE);

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeInt32(length, buffer + 4);
        Encoders::EncodeInt32(0, buffer + 8);   // No flags.
        Encoders::EncodeInt32(1, buffer + 12);  // Ask for the source address.
        Encoders::EncodeUInt32(0, buffer + 16); // No control data wanted.

        return (buffer);
    }

    /**
     * @brief Encode a close request into a newly allocated buffer.
     *
     * @param socketHandle Socket to close.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeCloseRequest(const int32_t socketHandle)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_CLOSE_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }

        Encoders::EncodeInt32(socketHandle, buffer);

        return (buffer);
    }

    /**
     * @brief Encode a poll request into a newly allocated buffer.
     *
     * @param socketHandle Socket being polled.
     * @param events NuttX poll events being asked for.
     * @param setup True to set the poll request up, false to tear it down.
     * @param setupMessageId Identifier correlating a teardown with its setup.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodePollRequest(const int32_t socketHandle, const uint16_t events, const bool setup, const uint32_t setupMessageId)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_POLL_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }

        Encoders::EncodeInt32(socketHandle, buffer);
        Encoders::EncodeUInt16(events, buffer + 4);
        Encoders::EncodeInt32(0, buffer + 6); // Timeout is not used by PollSetup or PollTeardown.
        Encoders::EncodeInt32(setup ? 1 : 0, buffer + 10);
        Encoders::EncodeUInt32(setupMessageId, buffer + 14);

        return (buffer);
    }

    /**
     * @brief Encode an ioctl request into a newly allocated buffer.
     *
     * @param command Ioctl command number.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeIoctlRequest(const int32_t command)
    {
        uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_IOCTL_REQUEST_SIZE));

        if (buffer == nullptr)
        {
            return (nullptr);
        }

        Encoders::EncodeInt32(command, buffer);

        return (buffer);
    }

    /**
     * @brief Extract the IoctlResponse object encoded in the given buffer.
     *
     * @param buffer Pointer to a block of memory holding the encoded IoctlResponse object.
     *
     * @return Pointer to an IoctlResponse object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::IoctlResponse *ExtractIoctlResponse(const uint8_t *buffer)
    {
        Esp32Messaging::IoctlResponse *ioctlResponse = static_cast<Esp32Messaging::IoctlResponse *>(pvPortMalloc(sizeof(Esp32Messaging::IoctlResponse)));

        if (ioctlResponse == nullptr)
        {
            return (nullptr);
        }

        ioctlResponse->AddrLength = Encoders::ExtractUInt32(buffer);
        buffer += 4;
        ioctlResponse->Addr = nullptr;
        if (ioctlResponse->AddrLength > 0)
        {
            ioctlResponse->Addr = static_cast<uint8_t *>(pvPortMalloc(ioctlResponse->AddrLength));
            if (ioctlResponse->Addr == nullptr)
            {
                Utils::DeleteAndNullifyPointer(ioctlResponse);
                return (nullptr);
            }
            memcpy(ioctlResponse->Addr, buffer, ioctlResponse->AddrLength);
            buffer += ioctlResponse->AddrLength;
        }
        ioctlResponse->Flags = Encoders::ExtractInt32(buffer);
        buffer += 4;
        ioctlResponse->Result = Encoders::ExtractInt32(buffer);
        buffer += 4;
        ioctlResponse->ResponseErrno = Encoders::ExtractInt32(buffer);

        return (ioctlResponse);
    }
} // namespace TestEncoders
