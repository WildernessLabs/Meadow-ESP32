/**
 * @file TestEncoders.hpp
 * @author Mark Stevens
 * @brief Encoding and extraction methods that mirror the Encoders class.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_ENCODERS_HPP_
#define _TEST_ENCODERS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "Encoders.hpp"

/**
 * @brief Encoding and extraction methods that complete the Encoders class for the tests.
 *
 * The Encoders class implements only the half of each conversion that the ESP32 itself
 * needs.  Objects the ESP32 sends to the STM32 can be encoded but not extracted, and
 * objects the STM32 sends to the ESP32 can be extracted but not encoded.  The tests stand
 * in for the STM32 and so need the opposite half of each pair, and those halves are
 * gathered here rather than being repeated in each test file.
 *
 * The naming follows the Encoders class so that the two read as a matching pair.
 */
namespace TestEncoders
{
    /**
     * @brief Extract the IntegerResponse object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling Utils::DeleteAndNullifyPointer(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded IntegerResponse object.
     *
     * @return Pointer to an IntegerResponse object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::IntegerResponse *ExtractIntegerResponse(const uint8_t *buffer);

    /**
     * @brief Extract the AccessPointList object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling FreeAccessPointList(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded AccessPointList object.
     *
     * @return Pointer to an AccessPointList object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::AccessPointList *ExtractAccessPointList(const uint8_t *buffer);

    /**
     * @brief Release an AccessPointList object and the access points it holds.
     *
     * @param accessPointList Pointer to the pointer to the object to be released.
     */
    void FreeAccessPointList(Esp32Messaging::AccessPointList **accessPointList);

    /**
     * @brief Extract the AccessPoint object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling Utils::DeleteAndNullifyPointer(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded AccessPoint object.
     *
     * @return Pointer to an AccessPoint object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::AccessPoint *ExtractAccessPoint(const uint8_t *buffer);

    /**
     * @brief Extract the FileNameList object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling FreeFileNameList(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded FileNameList object.
     *
     * @return Pointer to a FileNameList object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::FileNameList *ExtractFileNameList(const uint8_t *buffer);

    /**
     * @brief Release a FileNameList object and the file details it holds.
     *
     * @param fileNameList Pointer to the pointer to the object to be released.
     */
    void FreeFileNameList(Esp32Messaging::FileNameList **fileNameList);

    /**
     * @brief Extract the CoreDumpInformationResponse object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling Utils::DeleteAndNullifyPointer(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded object, nullptr for an
     *               empty response.
     *
     * @return Pointer to a CoreDumpInformationResponse object, nullptr if the object cannot
     *         be allocated.
     */
    Esp32Messaging::CoreDumpInformationResponse *ExtractCoreDumpInformation(const uint8_t *buffer);

    /**
     * @brief Calculate the memory required to hold an encoded CoreDumpFragmentRequest object.
     *
     * @param request Pointer to the CoreDumpFragmentRequest object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedCoreDumpFragmentRequestBufferSize(const Esp32Messaging::CoreDumpFragmentRequest * const request);

    /**
     * @brief Encode a CoreDumpFragmentRequest object into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling Utils::DeleteAndNullifyPointer(...).
     *
     * @param request Pointer to the CoreDumpFragmentRequest object to be encoded.
     *
     * @return Pointer to the encoded object, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeCoreDumpFragmentRequest(const Esp32Messaging::CoreDumpFragmentRequest * const request);

    /**
     * @brief Extract the CoreDumpFragmentResponse object encoded in the given buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap which should
     *       eventually be released by calling FreeCoreDumpFragmentResponse(...).
     *
     * @param buffer Pointer to a block of memory holding the encoded object.
     *
     * @return Pointer to a CoreDumpFragmentResponse object, nullptr if the object cannot be
     *         allocated.
     */
    Esp32Messaging::CoreDumpFragmentResponse *ExtractCoreDumpFragmentResponse(const uint8_t *buffer);

    /**
     * @brief Release a CoreDumpFragmentResponse object and the data it holds.
     *
     * @param fragmentResponse Pointer to the pointer to the object to be released.
     */
    void FreeCoreDumpFragmentResponse(Esp32Messaging::CoreDumpFragmentResponse **fragmentResponse);

    /**
     * @brief Calculate the memory required to hold an encoded BTStackConfig object.
     *
     * @param bTStackConfig Pointer to the BTStackConfig object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedBTStackConfigBufferSize(const Esp32Messaging::BTStackConfig * const bTStackConfig);

    /**
     * @brief Encode a BTStackConfig object into the given buffer.
     *
     * @note The buffer must be large enough to hold the encoded object, the size of which
     *       can be obtained by calling EncodedBTStackConfigBufferSize(...).
     *
     * @param bTStackConfig Pointer to the BTStackConfig object to be encoded.
     * @param buffer Pointer to the block of memory to hold the encoded object.
     */
    void EncodeBTStackConfig(const Esp32Messaging::BTStackConfig * const bTStackConfig, uint8_t *buffer);

    /**
     * @brief Calculate the memory required to hold an encoded BTServerDataSet object.
     *
     * @param bTServerDataSet Pointer to the BTServerDataSet object to be encoded.
     *
     * @return Number of bytes required to hold the encoded object.
     */
    uint32_t EncodedBTServerDataSetBufferSize(const Esp32Messaging::BTServerDataSet * const bTServerDataSet);

    /**
     * @brief Encode a BTServerDataSet object into the given buffer.
     *
     * @note The buffer must be large enough to hold the encoded object, the size of which
     *       can be obtained by calling EncodedBTServerDataSetBufferSize(...).
     *
     * @param bTServerDataSet Pointer to the BTServerDataSet object to be encoded.
     * @param buffer Pointer to the block of memory to hold the encoded object.
     */
    void EncodeBTServerDataSet(const Esp32Messaging::BTServerDataSet * const bTServerDataSet, uint8_t *buffer);

    /**
     * @brief Number of bytes in an encoded socket request.
     */
    const uint32_t ENCODED_SOCKET_REQUEST_SIZE = 16;

    /**
     * @brief Number of bytes in an encoded sockaddr structure.
     */
    const uint32_t ENCODED_SOCK_ADDR_SIZE = 31;

    /**
     * @brief Number of bytes in an encoded connect request.
     */
    const uint32_t ENCODED_CONNECT_REQUEST_SIZE = 8 + ENCODED_SOCK_ADDR_SIZE;

    /**
     * @brief Number of bytes in an encoded recvfrom request.
     */
    const uint32_t ENCODED_RECV_FROM_REQUEST_SIZE = 16;

    /**
     * @brief Number of bytes in an encoded recvmsg request.
     */
    const uint32_t ENCODED_RECV_MSG_REQUEST_SIZE = 20;

    /**
     * @brief Number of bytes in an encoded close request.
     */
    const uint32_t ENCODED_CLOSE_REQUEST_SIZE = 4;

    /**
     * @brief Address family for IPv4 using the NuttX numbering.
     *
     * @note NuttX and LwIP disagree on the value of AF_INET, and the STM32 encodes the
     *       NuttX value, so the tests must do the same.
     */
    const int32_t NUTTX_AF_INET = 2;

    /**
     * @brief Encode a socket request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param domain Address family for the socket.
     * @param type Type of socket required.
     * @param protocol Protocol the socket should use.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSocketRequest(const int32_t domain, const int32_t type, const int32_t protocol);

    /**
     * @brief Encode a connect request for an IPv4 address into a newly allocated buffer.
     *
     * @note The handler copies the port and the address into a sockaddr_in without
     *       converting them, so both are encoded in network byte order.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to connect.
     * @param address Dotted quad address of the server.
     * @param port Port to connect to, in host byte order.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeConnectRequest(const int32_t socketHandle, const char *address, const uint16_t port);

    /**
     * @brief Encode a send request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendRequest(const int32_t socketHandle, const char *data, const uint32_t length, uint32_t *encodedLength);

    /**
     * @brief Encode a sendto request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to send on.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param destinationAddress Encoded SockAddr bytes for the destination (e.g. from
     *      Encoders::EncodeSockAddr), or nullptr for no destination.
     * @param destinationAddressLength Length of destinationAddress, 0 for no destination.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendToRequest(const int32_t socketHandle, const char *data, const uint32_t length, const uint8_t *destinationAddress, const uint32_t destinationAddressLength, uint32_t *encodedLength);

    /**
     * @brief Encode a sendmsg request carrying a single iovec into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to send on.
     * @param name Raw NuttX sockaddr bytes for the destination name, or nullptr for none.
     * @param nameLength Length of name, 0 for none.
     * @param data Data to be sent.
     * @param length Number of bytes to send.
     * @param encodedLength Filled in with the length of the encoded request.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeSendMsgRequest(const int32_t socketHandle, const uint8_t *name, const uint32_t nameLength, const char *data, const uint32_t length, uint32_t *encodedLength);

    /**
     * @brief Encode a recvfrom request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to receive from.
     * @param length Largest number of bytes wanted.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeRecvFromRequest(const int32_t socketHandle, const int32_t length);

    /**
     * @brief Encode a recvmsg request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to receive from.
     * @param length Largest number of bytes wanted.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeRecvMsgRequest(const int32_t socketHandle, const int32_t length);

    /**
     * @brief Encode a close request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param socketHandle Socket to close.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeCloseRequest(const int32_t socketHandle);

    /**
     * @brief Size of the buffer EncodeIoctlRequest produces.
     */
    const uint32_t ENCODED_IOCTL_REQUEST_SIZE = 4;

    /**
     * @brief Encode an ioctl request into a newly allocated buffer.
     *
     * @note The returned pointer refers to a block of memory on the heap.  Ownership passes
     *       to the Message the request is attached to.
     *
     * @param command Ioctl command number, using the SIOCGIF* numbering WiFiRequestHandler.hpp
     *      defines for the wire format.
     *
     * @return Pointer to the encoded request, nullptr if the buffer cannot be allocated.
     */
    uint8_t *EncodeIoctlRequest(const int32_t command);

    /**
     * @brief Extract the IoctlResponse object encoded in the given buffer.
     *
     * @note The returned object's Addr field, when not null, is also on the heap and is the
     *       caller's to release.
     *
     * @param buffer Pointer to a block of memory holding the encoded IoctlResponse object.
     *
     * @return Pointer to an IoctlResponse object, nullptr if the object cannot be allocated.
     */
    Esp32Messaging::IoctlResponse *ExtractIoctlResponse(const uint8_t *buffer);
} // namespace TestEncoders

#endif // _TEST_ENCODERS_HPP_
