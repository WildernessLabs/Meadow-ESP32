/*
 *  PosixAPI.cpp
 *
 *  Implement POSIX APIs.
 */
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lwip/apps/sntp.h"
#include <inttypes.h>

#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Mapping.hpp"
#include "Logging.hpp"
#include "Utils.hpp"

/**
 * @brief Uncomment the line below to turn on assertions for debugging this file only.  Do not
 *        leave this enabled in production code as it is of no use to the user.
 */
// #undef NDEBUG
#include <assert.h>
#ifndef NDEBUG
    #pragma message "PosixAPIHandlers.cpp: Assertions enabled"
#endif

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Get address information.
 *
 * @param request Request from the STM32
 * @param response Response to the STM32
 * @return StatusCodes::StatusCodes Result of this method, StatusCodes::CompletedOk if the GetAddrInfo operation succeeds,
 *         StatusCodes::Failed if the GetAddrInfo operation fails.
 */
StatusCodes::StatusCodes WiFiRequestHandler::GetAddrInfo(Esp32Messaging::GetAddrInfoRequest *request, Esp32Messaging::GetAddrInfoResponse *response)
{
    Esp32Messaging::AddrInfo *extractedHints = Encoders::ExtractAddrInfo(request->Hints);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Hints));

    if (extractedHints == NULL)
    {
        return (StatusCodes::EspOutOfMemory);
    }

    struct addrinfo hints = {};
    hints.ai_family = extractedHints->Family;
    hints.ai_socktype = extractedHints->SocketType;

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&extractedHints));
    //
    //  This will hold a pointer to a chain of one or more addrinfo structures.
    //
    //  It is the responsibility of the calling .NET application to ensure
    //  that FreeAddrInfo is called.  FreeAddrInfo will free the allocated
    //  memory.
    //
    struct addrinfo *addressInformation;
    int errorCode = getaddrinfo(request->NodeName, request->ServName, &hints, &addressInformation);

    response->AddrInfoResponseErrno = Mapping::NuttxErrno(errorCode);
    if ((errorCode == 0) || (addressInformation != NULL))
    {
        Esp32Messaging::SockAddr sa = {};
        Esp32Messaging::AddrInfo a = {};

        Mapping::GetStmValue(Mapping::AddressFamily, addressInformation->ai_addr->sa_family, reinterpret_cast<int32_t *>(&sa.Family));
        Encoders::EncodeSockAddr(&sa, a.Addr);

        a.CanonName = strdup(addressInformation->ai_canonname);
        a.Family = addressInformation->ai_family;
        a.Flags = addressInformation->ai_flags;
        a.MyHeapAddress = reinterpret_cast<uint32_t>(addressInformation);
        a.Next = addressInformation->ai_next;
        a.Protocol = addressInformation->ai_protocol;
        a.SocketType = addressInformation->ai_socktype;
        a.AddrLen = addressInformation->ai_addrlen;

        response->ResLength = Encoders::EncodedAddrInfoBufferSize(&a);
        response->Res = static_cast<uint8_t *>(pvPortMalloc(response->ResLength));
        if (response->Res == NULL)
        {
            free(a.CanonName);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&addressInformation));
            return (StatusCodes::EspOutOfMemory);
        }
        Encoders::EncodeAddrInfo(&a, response->Res);
    }
    else
    {
        response->AddrInfoResponseErrno = Mapping::NuttxErrno(errno);
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&addressInformation));
    }
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX getaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::GetAddrInfo(void * const vpMessage)
{
    TRACE_MESSAGE("GetAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    Esp32Messaging::GetAddrInfoResponse *response = static_cast<Esp32Messaging::GetAddrInfoResponse *>(pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse)));
    if (response)
    {
        bzero(response, sizeof(Esp32Messaging::GetAddrInfoResponse));
        if (IsConnected())
        {
            Esp32Messaging::GetAddrInfoRequest *request = Encoders::ExtractGetAddrInfoRequest(message->Payload);
            if (request)
            {
                result = GetAddrInfo(request, response);
                //
                //  We assume that GetAddrInfo call above will release request->Hints.
                //
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
            }
            else
            {
                result = StatusCodes::EspOutOfMemory;
            }
        }
        else
        {
            response->AddrInfoResponseErrno = Mapping::NuttxErrno(ENETDOWN);
        }
    }
    else
    {
        result = StatusCodes::EspOutOfMemory;
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedGetAddrInfoResponseBufferSize(response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != NULL)
        {
            Encoders::EncodeGetAddrInfoResponse(response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response));
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX socket method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the socket method.
 */
void WiFiRequestHandler::Socket(void * const vpMessage)
{
    TRACE_MESSAGE("Socket: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    assert(message != NULL);
    assert(message->Payload != NULL);
    if (IsConnected())
    {
        Esp32Messaging::SocketRequest *request = Encoders::ExtractSocketRequest(message->Payload);
        if (request)
        {
            TRACE_MESSAGE("Socket: Creating socket on domain %" PRIi32 ", type %" PRIi32 " and protocol %" PRIi32, request->Domain, request->Type, request->Protocol);

            response.Result = socket(request->Domain, request->Type, request->Protocol);
            if (response.Result < 0)
            {
                TRACE_MESSAGE("Socket: Error code: %" PRIi32 " - %s", errno, strerror(errno));
                response.ResponseErrno = Mapping::NuttxErrno(errno);
            }
            else
            {
                TRACE_MESSAGE("Socket: Created socket %" PRIi32, response.Result);
                response.ResponseErrno = 0;
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != NULL)
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->Payload = nullptr;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Socket: Exit");
}

/**
 *  @brief Expose the POSIX connect method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the connect method.
 */
void WiFiRequestHandler::Connect(void * const vpMessage)
{
    TRACE_MESSAGE("Connect: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    assert(message != NULL);
    assert(message->Payload != NULL);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::ConnectRequest *request = Encoders::ExtractConnectRequest(message->Payload);
        if (request)
        {
            Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
            if (socketAddress)
            {
                if (request->AddrLength > 0)
                {
                    if (request->Addr)
                    {
                        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Addr));
                    }
                }

                struct sockaddr_in sa = {};
                int family;
                Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, reinterpret_cast<int32_t *>(&family));
                errno = 0;
                if (family == AF_INET)
                {
                    sa.sin_family = (uint8_t) (family & 0xff);
                    sa.sin_port = socketAddress->Port;
                    memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
                    sa.sin_len = sizeof(struct sockaddr_in);
                    TRACE_MESSAGE("Connect: Connecting socket: %ld", request->SocketHandle);
                    TRACE_MESSAGE("Connect: Family: %d", sa.sin_family);
                    TRACE_MESSAGE("Connect: IP address: %s", inet_ntoa(sa.sin_addr));
                    TRACE_MESSAGE("Connect: Port: %d", ntohs(sa.sin_port));
                    response.Result = connect(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
                    if (response.Result != 0)
                    {
                        TRACE_MESSAGE("Connect: Error code: %d - %s", errno, strerror(errno));
                        response.ResponseErrno = Mapping::NuttxErrno(errno);
                    }
                }
                else
                {
                    response.Result = -1;
                    response.ResponseErrno = Mapping::NuttxErrno(EFAULT);
                }
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&socketAddress));
            }
            else
            {
                result = StatusCodes::EspOutOfMemory;
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;

        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != NULL)
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Connect: Exit");
}

/**
 *  @brief Expose the POSIX freeaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the freeaddrinfo method.
 */
void WiFiRequestHandler::FreeAddrInfo(void * const vpMessage)
{
    TRACE_MESSAGE("FreeAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::FreeAddrInfoRequest *request = Encoders::ExtractFreeAddrInfoRequest(message->Payload);

    if (request != NULL)
    {
        freeaddrinfo(reinterpret_cast<struct addrinfo *>(request->AddrInfoAddress));
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        message->StatusCode = StatusCodes::CompletedOk;
    }
    else
    {
        message->StatusCode = StatusCodes::EspOutOfMemory;
    }

    message->DeletePayload();
    message->MessageType = MessageTypes::Response;

    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("FreeAddrInfo: Exit");
}

/**
 * @brief Process the SO_SNDTIMEO and SO_SNDTIMEO get socket option.
 *
 * @param vpMessage Pointer to the request.
 */
StatusCodes::StatusCodes WiFiRequestHandler::GetSendReceiveOption(int socketHandle, int level, int optionName, Esp32Messaging::GetSockOptResponse &response)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    Esp32Messaging::TimeVal esp_tv = {};
    struct timeval timeOut = {};

    socklen_t optionLength = sizeof(struct timeval);
    response.Result = getsockopt(socketHandle, level, optionName, static_cast<void *>(&timeOut), &optionLength);

    esp_tv.TvSec = timeOut.tv_sec;
    esp_tv.TvUsec = timeOut.tv_usec;
    response.OptionValueLength = Encoders::EncodedTimeValBufferSize(&esp_tv);
    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
    if (response.OptionValue == NULL)
    {
        response.ResponseErrno = Mapping::NuttxErrno(ENOBUFS);
        response.Result = -1;
        response.OptionValueLength = 0;
        result = StatusCodes::EspOutOfMemory;
    }
    else
    {
        Encoders::EncodeTimeVal(&esp_tv, response.OptionValue);
    }

    return (result);
}

/**
 * @brief Process the SO_LINGER get socket option.
 *
 * @param vpMessage Pointer to the request.
 */
StatusCodes::StatusCodes WiFiRequestHandler::GetLingerOption(int socket, int level, int optionName, Esp32Messaging::GetSockOptResponse &response)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    Esp32Messaging::Linger esp_lv = {};
    struct linger lingerOut = {};

    socklen_t optionLength = sizeof(struct linger);
    response.Result = getsockopt(socket, level, optionName, static_cast<void *>(&lingerOut), &optionLength);

    esp_lv.LLinger = lingerOut.l_linger;
    esp_lv.LOnOff = lingerOut.l_onoff;
    response.OptionValueLength = Encoders::EncodedLingerBufferSize(&esp_lv);
    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
    if (response.OptionValue == NULL)
    {
        response.ResponseErrno = Mapping::NuttxErrno(ENOBUFS);
        response.Result = -1;
        response.OptionValueLength = 0;
        result = StatusCodes::EspOutOfMemory;
    }
    else
    {
        Encoders::EncodeLinger(&esp_lv, response.OptionValue);
    }

    return (result);
}

/**
 * @brief Process the SO_RCVBUF get socket option.
 *
 * @param vpMessage Pointer to the request.
 */
StatusCodes::StatusCodes WiFiRequestHandler::GetSendReceiveBuffer(int socket, int level, int optionName, Esp32Messaging::GetSockOptResponse &response)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    Esp32Messaging::IntegerResponse esp_iv = {};

    int optionValue = 0;
    socklen_t optionLength = sizeof(int);
    response.Result = getsockopt(socket, level, optionName, static_cast<void *>(&optionValue), &optionLength);

    esp_iv.Result = optionValue;
    response.OptionValueLength = Encoders::EncodedIntegerResponseBufferSize(&esp_iv);
    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
    if (response.OptionValue == NULL)
    {
        response.ResponseErrno = Mapping::NuttxErrno(ENOBUFS);
        response.Result = -1;
        response.OptionValueLength = 0;
        result = StatusCodes::EspOutOfMemory;
    }
    else
    {
        Encoders::EncodeIntegerResponse(&esp_iv, response.OptionValue);
    }

    return (result);
}

/**
 *  @brief Expose the POSIX getsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockopt method.
 */
void WiFiRequestHandler::GetSockOpt(void * const vpMessage)
{
    TRACE_MESSAGE("GetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::GetSockOptResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::GetSockOptRequest *request = Encoders::ExtractGetSockOptRequest(message->Payload);
        if (request)
        {
            int32_t optionName = 0;
            if (Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName) < 0)
            {
                TRACE_MESSAGE("GetSockOpt: Unknown option: %" PRIi32, request->OptionName);
                response.Result = -1;
                response.ResponseErrno = Mapping::NuttxErrno(ENOPROTOOPT);
            }
            else
            {
                int32_t level = 0;
                Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
                TRACE_MESSAGE("GetSockOpt: Getting socket option level 0x%" PRIx32 ", option name 0x%" PRIx32 " for socket %" PRIi32, (uint) level, (uint) optionName, request->SocketHandle);
                response.Result = -1;
                response.ResponseErrno = Mapping::NuttxErrno(ENOPROTOOPT);
                switch (optionName)
                {
                    case SO_SNDTIMEO:   /* Get the send timeout */
                    case SO_RCVTIMEO:   /* Get the receive timeout */
                        result = GetSendReceiveOption(request->SocketHandle, level, optionName, response);
                        break;
                    case SO_LINGER:     /* Linger on close if data present */
                        result = GetLingerOption(request->SocketHandle, level, optionName, response);
                        break;
                    case SO_RCVBUF:     /* Get the receive buffer size */
                        result = GetSendReceiveBuffer(request->SocketHandle, level, optionName, response);
                        break;
                    case SO_ACCEPTCONN: /* Socket has had listen() */
                        TRACE_MESSAGE("GetSockOpt: Implement SO_ACCEPTCONN");
                        break;
                    case SO_DONTLINGER: /* Don't linger on close if data present */
                        TRACE_MESSAGE("GetSockOpt: Implement SO_DONTLINGER");
                        break;
                    case SO_ERROR:      /* Get error status and clear */
                        TRACE_MESSAGE("GetSockOpt: Implement SO_ERROR");
                        break;
                    case SO_TYPE:       /* Get socket type */
                        TRACE_MESSAGE("GetSockOpt: Implement SO_TYPE");
                        break;
                    case SO_NO_CHECK:   /* Don't create UDP checksum */
                        TRACE_MESSAGE("GetSockOpt: Implement SO_NO_CHECK");
                        break;
                    default:
                        TRACE_MESSAGE("GetSockOpt: Unknown option: %" PRIi32, request->OptionName);
                        break;
                }
            }
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
            result = StatusCodes::EspOutOfMemory;
        }
        TRACE_MESSAGE("GetSockOpt: Result: %" PRIi32 ", errno: %" PRIi32 " - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    if (result != StatusCodes::CompletedOk)
    {
        message->DeletePayload();
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedGetSockOptResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != NULL)
        {
            Encoders::EncodeGetSockOptResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        if (response.OptionValue != NULL)
        {
            free(response.OptionValue);
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockOpt: Exit");
}

/**
 * @brief Extract an encoded integer setsockopt option value.
 *
 * @param data
 *      Source of the socket option data (the option value).
 *
 * @param length
 *      Pointer to a socklen_t object to take the length of the extracted option value.
 *
 * @return uint8_t*
 *      Pointer to the extracted option value (integer) or NULL if there was a problem.
 */
uint8_t *WiFiRequestHandler::ExtractIntegerSocketOptionValue(uint8_t *data, socklen_t *length)
{
    uint8_t *result = NULL;

    if (length != NULL)
    {
        *length = 0;
        if (data != NULL)
        {
            int *intVal = reinterpret_cast<int *>(pvPortMalloc(sizeof(int)));
            if (intVal != NULL)
            {
                *intVal = Encoders::ExtractInt32(data);
                *length = sizeof(intVal);
            }
            result = reinterpret_cast<uint8_t *>(intVal);
        }
    }

    return (result);
}

/**
 * @brief Setup the socket value and length for calls to SetSockOpt.
 *
 * @param name
 *      Name of the option to decode.
 *
 * @param optionValue
 *      Pointer to the source data.
 *
 * @param length
 *      Pointer to a socklen_t object to take the length of the option value.
 *
 * @return uint8_t *
 *      Pointer the the decoded option value.
 */
uint8_t *WiFiRequestHandler::GetSocketOptionValue(int name, uint8_t *optionValue, socklen_t *length)
{
    TRACE_MESSAGE("GetSocketOptionValue: Enter");

    uint8_t *result = NULL;
    if (length != NULL)
    {
        switch (name)
        {
            case SO_SNDTIMEO: /* Set the send timeout */
            case SO_RCVTIMEO: /* Set the receive timeout */
                {
                    struct timeval *value = reinterpret_cast<struct timeval *>(pvPortMalloc(sizeof(struct timeval)));
                    if (value != NULL)
                    {
                        Esp32Messaging::TimeVal *tv = Encoders::ExtractTimeVal(optionValue);
                        if (tv == NULL)
                        {
                            *length = 0;
                            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&value));
                        }
                        else
                        {
                            value->tv_sec = tv->TvSec;
                            value->tv_usec = tv->TvUsec;
                            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&tv));
                            result = reinterpret_cast<uint8_t *>(value);
                            *length = sizeof(struct timeval);
                        }
                    }
                }
                break;
            case SO_REUSEADDR:  /* Reuse local addresses when binding */
            case SO_RCVBUF:     /* Set the receive buffer size */
                result = ExtractIntegerSocketOptionValue(optionValue, length);
                break;
            case SO_LINGER:     /* Linger on close if data present */
                TRACE_MESSAGE("SetSockOpt: Implement SO_LINGER");
                break;
            case SO_ACCEPTCONN: /* Socket has had listen() */
                TRACE_MESSAGE("SetSockOpt: Implement SO_ACCEPTCONN");
                break;
            case SO_DONTLINGER: /* Don't linger on close if data present */
                TRACE_MESSAGE("SetSockOpt: Implement SO_DONTLINGER");
                break;
            case SO_ERROR:      /* Get error status and clear */
                TRACE_MESSAGE("SetSockOpt: Implement SO_ERROR");
                break;
            case SO_TYPE:       /* Get socket type */
                TRACE_MESSAGE("SetSockOpt: Implement SO_TYPE");
                break;
            case SO_NO_CHECK:   /* Don't create UDP checksum */
                TRACE_MESSAGE("SetSockOpt: Implement SO_NO_CHECK");
                break;
            case SO_BROADCAST:
                TRACE_MESSAGE("SetSockOpt: Implement SO_BROADCAST");
                break;
            case SO_KEEPALIVE:
                TRACE_MESSAGE("SetSockOpt: Implement SO_KEEPALIVE");
                break;
            default:
                break;
        }
    }

    TRACE_MESSAGE("GetSocketOptionValue: Exit");

    return (result);
}

/**
 * @brief Setup the TCP value and length for calls to SetSockOpt.
 *
 * @param name
 *      Name of the option to decode.
 *
 * @param optionValue
 *      Pointer to the source data.
 *
 * @param length
 *      Pointer to a socklen_t object to take the length of the option value.
 *
 * @return uint8_t *
 *      Pointer the the decoded option value.
 */
uint8_t *WiFiRequestHandler::GetTcpOptionValue(int name, uint8_t *optionValue, socklen_t *length)
{
    TRACE_MESSAGE("GetTcpOptionValue: Enter");

    uint8_t *result = NULL;

    if (length != NULL)
    {
        switch (name)
        {
            case TCP_NODELAY:
                result = ExtractIntegerSocketOptionValue(optionValue, length);
                break;
            default:
                *length = 0;
                break;
        }
    }

    TRACE_MESSAGE("GetTcpOptionValue: Exit");

    return (result);
}

/**
 * @brief Process the setsockopt request from the STM32.
 *
 * @param request Request from the STM32.
 * @param response Response to the STM32.
 *
 * @return StatusCodes::StatusCodes Status of the request.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetSockOpt(Esp32Messaging::SetSockOptRequest *request, Esp32Messaging::IntegerAndErrnoResponse &response)
{
    void *optionValue = nullptr;
    socklen_t optionLength = 0;
    int32_t level = 0;

    if (Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level) < 0)
    {
        TRACE_MESSAGE("SetSockOpt: Unknown level: 0x%" PRIx32, request->Level);
    }
    else
    {
        int32_t optionName = 0;
        if (Mapping::GetEspValue(level == SOL_SOCKET ? Mapping::SocketOptions : Mapping::TCPOptions, request->OptionName, &optionName) < 0)
        {
            TRACE_MESSAGE("SetSockOpt: Unknown option name: 0x%x", (uint) request->OptionName);
        }

        switch (level)
        {
            case SOL_SOCKET:
                optionValue = GetSocketOptionValue(optionName, request->OptionValue, &optionLength);
                break;
            case IPPROTO_TCP:
                optionValue = GetTcpOptionValue(optionName, request->OptionValue, &optionLength);
                break;
            default:
                TRACE_MESSAGE("SetSockOpt: Cannot process option level: 0x%x", (uint) request->Level);
                break;
        }
        if (optionValue != NULL)
        {
            TRACE_MESSAGE("SetSockOpt: Setting socket option level to 0x%x, option name 0x%x for socket %ld", (uint) level, (uint) optionName, request->SocketHandle);
            response.Result = setsockopt(request->SocketHandle, level, optionName, optionValue, optionLength);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&optionValue));
        }
    }

    if (response.Result != 0)
    {
        response.ResponseErrno = Mapping::NuttxErrno(errno);
    }
    TRACE_MESSAGE("SetSockOpt: Result: %ld, errno: %ld - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX setsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the setsockopt method.
 */
void WiFiRequestHandler::SetSockOpt(void * const vpMessage)
{
    TRACE_MESSAGE("SetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::SetSockOptRequest *request = Encoders::ExtractSetSockOptRequest(message->Payload);
        if (request)
        {
            result = SetSockOpt(request, response);
            if (request->OptionValueLength > 0)
            {
                if (request->OptionValue)
                {
                    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->OptionValue));
                }
            }
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
            result = StatusCodes::EspOutOfMemory;
        }

        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SetSockOpt: Exit");
}

/**
 *  @brief Expose the POSIX write method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the write method.
 */
void WiFiRequestHandler::Write(void * const vpMessage)
{
    TRACE_MESSAGE("Write: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::WriteRequest *request = Encoders::ExtractWriteRequest(message->Payload);
        if (request)
        {
            response.Result = write(request->SocketHandle, request->Buffer, request->Count);
            if (response.Result < 0)
            {
                response.ResponseErrno = Mapping::NuttxErrno(errno);
            }

            if ((request->BufferLength > 0) && request->Buffer)
            {
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Buffer));
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Write: Exit");
}

/**
 *  @brief Expose the POSIX read method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the read method.
 */
void WiFiRequestHandler::Read(void * const vpMessage)
{
    TRACE_MESSAGE("Read: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);

    Esp32Messaging::ReadResponse response = {};
    response.Buffer = nullptr;
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::ReadRequest *request = Encoders::ExtractReadRequest(message->Payload);
        if (request)
        {
            response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Count));
            if (response.Buffer)
            {
                bzero(response.Buffer, request->Count);
                response.BufferLength = request->Count;
                response.ReadResponseResult = read(request->SocketHandle, response.Buffer, request->Count);
                if (response.ReadResponseResult < 0)
                {
                    response.ReadResponseErrno = Mapping::NuttxErrno(errno);
                    free(response.Buffer);
                    response.Buffer = NULL;
                    response.BufferLength = 0;
                }
                else
                {
                    result = StatusCodes::EspOutOfMemory;
                }
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.ReadResponseResult = -1;
        response.ReadResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedReadResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeReadResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Buffer));

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Read: Exit");
}

/**
 *  @brief Expose the POSIX close method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the close method.
 */
void WiFiRequestHandler::Close(void * const vpMessage)
{
    TRACE_MESSAGE("Close: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    assert(message != NULL);
    assert(message->Payload != NULL);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::CloseRequest *request = Encoders::ExtractCloseRequest(message->Payload);
        if (request)
        {
            response.Result = close(request->SocketHandle);
            if (response.Result < 0)
            {
                response.ResponseErrno = Mapping::NuttxErrno(errno);
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        message->MessageType = MessageTypes::Response;
        _messageDispatcher->QueueMessageForStm32(message);
    }

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Close: Exit");
}

/**
 *  @brief Expose the POSIX send method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the send method.
 */
void WiFiRequestHandler::Send(void * const vpMessage)
{
    TRACE_MESSAGE("Send: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::SendRequest *request = Encoders::ExtractSendRequest(message->Payload);
        if (request)
        {
            int32_t mappedFlags = 0;
            response.ResponseErrno = 0;
            response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
            if (response.Result == 0)
            {
                response.Result = send(request->SocketHandle, request->Buffer, request->Length, mappedFlags);
                if (response.Result < 0)
                {
                    response.ResponseErrno = Mapping::NuttxErrno(errno);
                }
            }
            else
            {
                response.ResponseErrno = Mapping::NuttxErrno(EFAULT);
            }

            TRACE_MESSAGE("Send: Sent %" PRIi32 " bytes.", request->Length);
            if (request->Buffer && (request->Length > 0))
            {
                TRACE_HEX_BUFFER(request->Buffer, request->Length);
            }
            TRACE_MESSAGE("Send: Result: %" PRIi32 ", errno: %" PRIi32 " - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Buffer));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Send: Exit");
}

/**
 * @brief Provide a mechanism for the STM32 to call the sendto method.
 *
 * @param request Request from the STM32.
 * @param response Response to the STM32.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SendTo(Esp32Messaging::SendToRequest *request, Esp32Messaging::IntegerAndErrnoResponse &response)
{
    int32_t mappedFlags = 0;
    response.ResponseErrno = 0;
    response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
    if (response.Result == 0)
    {
        Esp32Messaging::SockAddr *socketAddress = NULL;
        int32_t family = 0;
        if (request->DestinationAddress)
        {
            socketAddress = Encoders::ExtractSockAddr(request->DestinationAddress);
            if (socketAddress)
            {
                Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
                TRACE_MESSAGE("SendTo: Mapping request family %" PRIu8 " to ESP family %" PRIi32, socketAddress->Family, family);
            }
            else
            {
                response.Result = -1;
                response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
                return (StatusCodes::EspOutOfMemory);
            }
        }

        struct sockaddr_in sa = {};
        struct sockaddr *psa = NULL;
        size_t lenSa = 0;
        if (request->DestinationAddressLength != 0)
        {
            if (family == AF_INET)
            {
                if (request->DestinationAddress)
                {
                    sa.sin_family = (unsigned char) (family & 0xff);

                    sa.sin_port = socketAddress->Port;
                    memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
                    sa.sin_len = sizeof(sa);
                    psa = (struct sockaddr *) &sa;
                    lenSa = sizeof(sa);
                }
                TRACE_MESSAGE("SendTo: Connecting socket: %" PRIi32, request->SocketHandle);
                TRACE_MESSAGE("SendTo: Family: %" PRIi32, (psa == NULL) ? 0 : sa.sin_family);
                TRACE_MESSAGE("SendTo: IP address: %s", (psa == NULL) ? "NULL" : inet_ntoa(sa.sin_addr));
                TRACE_MESSAGE("SendTo: Port: %" PRIi32, (psa == NULL) ? 0 : ntohs(sa.sin_port));
            }
        }
        response.Result = sendto(request->SocketHandle, request->Buffer, request->Length, mappedFlags, psa, lenSa);

        if (response.Result < 0)
        {
            response.ResponseErrno = Mapping::NuttxErrno(errno);
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&socketAddress));
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(EFAULT);
        return (StatusCodes::Failure);
    }
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX sendto method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the sendto method.
 */
void WiFiRequestHandler::SendTo(void * const vpMessage)
{
    TRACE_MESSAGE("SendTo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::SendToRequest *request = Encoders::ExtractSendToRequest(message->Payload);
        if (request)
        {
            result = SendTo(request, response);
            TRACE_MESSAGE("SendTo: Sent %" PRIi32 " bytes.", response.Result);
            if (request->Buffer && (request->Length > 0))
            {
                TRACE_HEX_BUFFER(request->Buffer, request->Length);
            }
            TRACE_MESSAGE("SendTo: Result: %" PRIi32 ", errno: %" PRIi32 " - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->DestinationAddress));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Buffer));
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }
    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SendTo: Exit");
}

/**
 *  @brief Process the recvfrom request from the STM32.
 *
 *  @param request Request from the STM32.
 *  @param response Response to the STM32.
 *  @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
 *          StatusCodes::EspOutOfMemory if memory allocation fails.
 */
StatusCodes::StatusCodes WiFiRequestHandler::RecvFrom(Esp32Messaging::RecvFromRequest *request, Esp32Messaging::RecvFromResponse &response)
{
    int32_t mappedFlags = 0;
    response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
    if (response.Result == 0)
    {
        TRACE_MESSAGE("Requesting %" PRIi32 " bytes from socket handle %" PRIi32, request->Length, request->SocketHandle);
        struct sockaddr_in *sa = NULL;
        socklen_t len;
        if (request->GetSourceAddress != 0)
        {
            sa = static_cast<struct sockaddr_in *>(pvPortMalloc(sizeof(struct sockaddr_in)));
            if (sa == NULL)
            {
                response.Buffer = nullptr;
                response.Result = -1;
                response.ResponseErrno = ENOMEM;
                return (StatusCodes::EspOutOfMemory);
            }
            bzero(sa, sizeof(struct sockaddr_in));
            len = sizeof(struct sockaddr_in);
        }
        response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Length));
        if (response.Buffer == NULL)
        {
            response.Buffer = nullptr;
            response.Result = -1;
            response.ResponseErrno = ENOMEM;
            return (StatusCodes::EspOutOfMemory);
        }
        else
        {
            response.Result = recvfrom(request->SocketHandle, response.Buffer, request->Length, mappedFlags, reinterpret_cast<struct sockaddr *>(sa), (sa == NULL) ? NULL : &len);
        }

        if (response.Result < 0)
        {
            response.ResponseErrno = errno;
        }
        else
        {
            if (request->GetSourceAddress != 0)
            {
                Esp32Messaging::SockAddr socketAddress = {};
                Mapping::GetStmValue(Mapping::AddressFamily, sa->sin_family, reinterpret_cast<int32_t *>(&socketAddress.Family));
                memcpy(&socketAddress.Ip4Address, &sa->sin_addr, sizeof(sa->sin_addr));
                socketAddress.Port = sa->sin_port;
                response.SourceAddressLength = Encoders::EncodedSockAddrBufferSize(&socketAddress);
                response.SourceAddress = static_cast<uint8_t *>(pvPortMalloc(response.SourceAddressLength));
                if (response.SourceAddress == NULL)
                {
                    response.SourceAddressLength = 0;
                    response.SourceAddressLen = 0;
                    response.Result = -1;
                    response.ResponseErrno = ENOMEM;
                    return (StatusCodes::EspOutOfMemory);
                }
                else
                {
                    Encoders::EncodeSockAddr(&socketAddress, response.SourceAddress);
                }
            }
            else
            {
                response.SourceAddress = nullptr;
                response.SourceAddressLength = 0;
                response.SourceAddressLen = 0;
            }
            response.BufferLength = response.Result;
        }

        if (sa != NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sa));
        }
    }
    else
    {
        response.ResponseErrno = EFAULT;
    }
    TRACE_MESSAGE("RecvFrom: Received %" PRIi32 " bytes.", response.Result);
    if (response.Buffer && (response.BufferLength > 0))
    {
        TRACE_HEX_BUFFER(response.Buffer, response.Result);
    }
    TRACE_MESSAGE("RecvFrom: Result: %" PRIi32 ", errno: %" PRIi32 " - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
    if (response.Result < 0)
    {
        response.ResponseErrno = Mapping::NuttxErrno(response.ResponseErrno);
    }
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX recvfrom method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the recvfrom method.
 */
void WiFiRequestHandler::RecvFrom(void * const vpMessage)
{
    TRACE_MESSAGE("RecvFrom: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::RecvFromResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::RecvFromRequest *request = Encoders::ExtractRecvFromRequest(message->Payload);
        if (request)
        {
            result = RecvFrom(request, response);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedRecvFromResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeRecvFromResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Buffer));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.SourceAddress));

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("RecvFrom: Exit");
}

/**
 * @brief Bind a socket to an address.
 *
 * @param request Request from the STM32.
 * @param response Response to the STM32.
 * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
 *         StatusCodes::EspOutOfMemory if memory allocation fails.
 */
StatusCodes::StatusCodes WiFiRequestHandler::Bind(Esp32Messaging::BindRequest *request, Esp32Messaging::IntegerAndErrnoResponse &response)
{
    Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
    if (socketAddress == NULL)
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
        return (StatusCodes::EspOutOfMemory);
    }

    if (request->AddrLength > 0)
    {
        if (request->Addr)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request->Addr));
        }
    }

    struct sockaddr_in sa = {};
    int family;
    Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, reinterpret_cast<int32_t *>(&family));
    errno = 0;
    if (family == AF_INET)
    {
        sa.sin_family = socketAddress->Family;
        sa.sin_port = socketAddress->Port;
        memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(socketAddress->Ip4Address));
        TRACE_MESSAGE("Bind: Socket handle: %" PRIi32, request->SocketHandle);
        TRACE_MESSAGE("Bind: Family: %" PRIi32, sa.sin_family);
        TRACE_MESSAGE("Bind: IP address: %s", inet_ntoa(sa.sin_addr));
        TRACE_MESSAGE("Bind: Port: %" PRIi16, ntohs(sa.sin_port));
        response.Result = bind(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
        if (response.Result != 0)
        {
            response.ResponseErrno = Mapping::NuttxErrno(errno);
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(EFAULT);
    }

    TRACE_MESSAGE("Bind: Error code: %" PRIi32 " - %s", errno, strerror(errno));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&socketAddress));

    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX bind method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the bind method.
 */
void WiFiRequestHandler::Bind(void * const vpMessage)
{
    TRACE_MESSAGE("Bind: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::BindRequest *request = Encoders::ExtractBindRequest(message->Payload);
        if (request)
        {
            result = Bind(request, response);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Bind: Exit");
}

/**
 *  @brief Expose the POSIX listen method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the listen method.
 */
void WiFiRequestHandler::Listen(void * const vpMessage)
{
    TRACE_MESSAGE("Listen: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    assert(message != NULL);
    assert(message->Payload != NULL);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::ListenRequest *request = Encoders::ExtractListenRequest(message->Payload);
        if (request)
        {
            response.Result = listen(request->SocketHandle, request->BackLog);
            response.ResponseErrno = (response.Result == 0) ? 0 : Mapping::NuttxErrno(errno);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    PERFORMANCE_LOGGING_DUMP_RESULTS();
    TRACE_MESSAGE("Listen: Exit");
}

/**
 * @brief Process the accept request from the STM32.
 *
 * @param request Request from the STM32.
 * @param response Response to the STM32.
 *
 * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
 *         StatusCodes::EspOutOfMemory if memory allocation fails.
 */
StatusCodes::StatusCodes WiFiRequestHandler::Accept(Esp32Messaging::AcceptRequest *request, Esp32Messaging::AcceptResponse &response)
{
    struct sockaddr sa = {};
    socklen_t saLen = sizeof(sa);
    response.Result = accept(request->SocketHandle, &sa, &saLen);
    if (response.Result > 0)
    {
        Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
        if (sockAddr)
        {
            bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
            Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, reinterpret_cast<int32_t *>(&sockAddr->Family));
            struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
            sockAddr->Port = sin->sin_port;
            memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
            response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
            response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
            if (response.Addr == NULL)
            {
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sockAddr));
                return (StatusCodes::EspOutOfMemory);
            }
            Encoders::EncodeSockAddr(sockAddr, response.Addr);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sockAddr));
            response.ResponseErrno = 0;
            TRACE_MESSAGE("Accept: Socket handle: %ld", response.Result);
            TRACE_MESSAGE("Accept: Family: %d", sin->sin_family);
            TRACE_MESSAGE("Accept: IP address: %s", inet_ntoa(sin->sin_addr));
            TRACE_MESSAGE("Accept: Port: %d", ntohs(sin->sin_port));
        }
        else
        {
            return (StatusCodes::EspOutOfMemory);
        }
    }
    else
    {
        response.AddrLength = 0;
        response.Addr = nullptr;
        response.ResponseErrno = Mapping::NuttxErrno(errno);
    }
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Expose the POSIX accept method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the accept method.
 */
void WiFiRequestHandler::Accept(void * const vpMessage)
{
    TRACE_MESSAGE("Accept: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);
    assert(message != NULL);
    assert(message->Payload != NULL);

    Esp32Messaging::AcceptResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::AcceptRequest *request = Encoders::ExtractAcceptRequest(message->Payload);
        if (request)
        {
            result = Accept(request, response);
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedAcceptResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeAcceptResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Addr));
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Accept: Exit");
}

/**
 *  @brief Expose the simulation of the POSIX ioctl method to the STM32.
 *
 *  This is a simulation of the ioctl method as this is not supported by the
 *  ESP-IDF framework.
 *
 *  @param message
 *      Message structure containing the parameters for the ioctl method.
 */
void WiFiRequestHandler::Ioctl(void * const vpMessage)
{
    TRACE_MESSAGE("Ioctl: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IoctlResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::IoctlRequest *request = Encoders::ExtractIoctlRequest(message->Payload);

        esp_netif_ip_info_t ipInfo;
        Esp32Messaging::SockAddr sa = {};

        Mapping::GetStmValue(Mapping::AddressFamily, AF_INET, reinterpret_cast<int32_t *>(&sa.Family));
        response.AddrLength = 0;
        response.Addr = nullptr;

        switch (request->Command)
        {
            case SIOCGIFNETMASK: /* Get network mask */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.netmask, sizeof(ipInfo.netmask));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    if (response.Addr == NULL)
                    {
                        response.Result = -1;
                        response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
                        result = StatusCodes::EspOutOfMemory;
                    }
                    else
                    {
                        Encoders::EncodeSockAddr(&sa, response.Addr);
                    }
                }
                break;
            case SIOCGIFHWADDR: /* Get hardware address */
                response.AddrLength = 6;
                response.Addr = static_cast<uint8_t *>(pvPortMalloc(6));
                if (response.Addr == NULL)
                {
                    response.Result = -1;
                    response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
                    result = StatusCodes::EspOutOfMemory;
                }
                else
                {
                    GetMacAddress(response.Addr, WIFI_IF_STA);
                }
                break;
            case SIOCGIFADDR: /* Get IP address */
            case SIOCGIFCONF: /* Return an interface list (IPv4) */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.ip, sizeof(ipInfo.ip));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    if (response.Addr == NULL)
                    {
                        response.Result = -1;
                        response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
                        result = StatusCodes::EspOutOfMemory;
                    }
                    else
                    {
                        Encoders::EncodeSockAddr(&sa, response.Addr);
                    }
                }
                break;
            case SIOCGIFFLAGS: /* Gets the interface flags */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    response.Flags = IFF_UP;
                }
                else
                {
                    response.Flags = IFF_DOWN;
                }
                break;
            default:
                response.Result = -1;
                response.ResponseErrno = Mapping::NuttxErrno(EINVAL);
                TRACE_MESSAGE("Unknown ioctl request %04" PRIx16, (uint) request->Command);
                break;
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIoctlResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIoctlResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Addr));
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Ioctl: Exit");
}

/**
 * @brief Provide a mechanism to call the getsockname or getpeername.
 *
 * @param request Request from the STM32.
 * @param methodToCall Indicate if we are calling getsockname or getpeername.
 * @param response Response to the STM32.
 *
 * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
 *         StatusCodes::EspOutOfMemory if memory allocation fails.
 */
StatusCodes::StatusCodes WiFiRequestHandler::GetSockPeerName(Esp32Messaging::GetSockPeerNameRequest *request, WiFiFunction::WiFiFunction methodToCall, Esp32Messaging::GetSockPeerNameResponse &response)
{
    struct sockaddr sa = {};
    socklen_t saLen = sizeof(sa);
    if (methodToCall == WiFiFunction::GetSockName)
    {
        response.Result = getsockname(request->SocketHandle, &sa, &saLen);
    }
    else
    {
        response.Result = getpeername(request->SocketHandle, &sa, &saLen);
    }
    if (response.Result == 0)
    {
        Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
        if (sockAddr == NULL)
        {
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
            return (StatusCodes::EspOutOfMemory);
        }
        bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
        Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, reinterpret_cast<int32_t *>(&sockAddr->Family));
        struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
        sockAddr->Port = sin->sin_port;
        memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
        response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
        response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
        if (response.Addr == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sockAddr));
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
            return (StatusCodes::EspOutOfMemory);
        }
        Encoders::EncodeSockAddr(sockAddr, response.Addr);
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&sockAddr));
        response.ResponseErrno = 0;
        TRACE_MESSAGE("GetSockPeerName: Socket handle: %" PRIi32, request->SocketHandle);
        TRACE_MESSAGE("GetSockPeerName: Family: %" PRIi32, sin->sin_family);
        TRACE_MESSAGE("GetSockPeerName: IP address: %s", inet_ntoa(sin->sin_addr));
        TRACE_MESSAGE("GetSockPeerName: Port: %" PRIi32, ntohs(sin->sin_port));
    }
    else
    {
        response.AddrLength = 0;
        response.Addr = nullptr;
        response.ResponseErrno = Mapping::NuttxErrno(errno);
        ERROR_MESSAGE("Failure: Result code %" PRIi32 " (%s), errno %" PRIi32 " (%s)", response.Result, esp_err_to_name(response.Result), errno, strerror(errno));
    }
    return StatusCodes::CompletedOk;
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 *
 *  @param methodToCall
 *      Which method should be called, getsockname or getpeername?
 */
void WiFiRequestHandler::GetSockPeerName(void * const vpMessage, WiFiFunction::WiFiFunction methodToCall)
{
    TRACE_MESSAGE("GetSockPeerName: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::GetSockPeerNameResponse response = {};
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::GetSockPeerNameRequest *request = Encoders::ExtractGetSockPeerNameRequest(message->Payload);
        if (request)
        {
            result = GetSockPeerName(request, methodToCall, response);
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = Mapping::NuttxErrno(ENOMEM);
            result = StatusCodes::EspOutOfMemory;
        }

        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();
    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedGetSockPeerNameResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeGetSockPeerNameResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Addr));
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockPeerName: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 */
void WiFiRequestHandler::GetSockName(void * const vpMessage)
{
    TRACE_MESSAGE("GetSockName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetSockName);
    TRACE_MESSAGE("GetSockName: Exit");
}

/**
 *  @brief Expose the POSIX getpeername method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getpeername method.
 */
void WiFiRequestHandler::GetPeerName(void * const vpMessage)
{
    TRACE_MESSAGE("GetPeerName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetPeerName);
    TRACE_MESSAGE("GetPeerName: Exit");
}

/**
 *  @brief Lock the _pollRequests object.
 */
void WiFiRequestHandler::LockPollRequests()
{
    xSemaphoreTake(_pollRequestListMutex, portMAX_DELAY);
}

/**
 *  @brief Unlock the _pollRequests object.
 */
void WiFiRequestHandler::UnlockPollRequests()
{
    xSemaphoreGive(_pollRequestListMutex);
}

/**
 *  @brief Open a dummy socket to be used by the Poll methods to terminate a poll request.
 *
 *  @returns
 *      Handle to the socket that has just been opened of -1 if there is a problem.
 */
int WiFiRequestHandler::OpenDummySocket()
{
    TRACE_MESSAGE("OpenDummySocket: Enter");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;

    int dummy_socket_fd = -1;
    if (getaddrinfo("localhost", "80", &hints, &res) >= 0)
    {
        dummy_socket_fd = socket(res->ai_family, res->ai_socktype, 0);
        freeaddrinfo(res);
    }
    TRACE_MESSAGE("OpenDummySocket: Exit, result %d", dummy_socket_fd);

    return (dummy_socket_fd);
}

/**
 *  @brief Debug method to dump all of the active poll requests.
 */
void WiFiRequestHandler::DumpActivePollRequests()
{
    TRACE_MESSAGE("DumpActivePollRequests: Enter");
    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.begin();
    while (it != _pollRequests.end())
    {
        //
        //  fds is marked as unused as in debug mode the TRACE_MESSAGE results in the usage being thrown away.
        //  Basically, we are trying to keep the compiler happy and not generate misleading warnings.
        //
        // cppcheck-suppress unreadVariable symbolName=fds
        __attribute__((unused)) struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        TRACE_MESSAGE("DumpActiveRequests: Key %08x, socket %d, dummy socket %d", (uint) it->first, fds[POLL_FD_INDEX].fd, fds[POLL_DUMMY_FD_INDEX].fd);
        it++;
    }
    UnlockPollRequests();
    TRACE_MESSAGE("DumpActivePollRequests: Exit");
}

/**
 *  @brief Setup a poll request an assign it to a thread in the thread pool.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollSetup(void * const vpMessage)
{
    TRACE_MESSAGE("PollSetup: Enter");

    int result = 0;
    if (vpMessage == NULL)
    {
        return (-1);
    }
    struct pollfd *fds = static_cast<struct pollfd *>(pvPortMalloc(2 * sizeof(struct pollfd)));
    if (fds == NULL)
    {
        return (-1);
    }

    int dummy_socket_handle = OpenDummySocket();
    if (dummy_socket_handle < 0)
    {
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fds));
        return (-1);
    }

    Esp32Messaging::PollRequest * const request = static_cast<Esp32Messaging::PollRequest *>(vpMessage);
    memset(fds, 0, 2 * sizeof(struct pollfd));
    int32_t events;
    Mapping::GetEspFlagValue(Mapping::PollEvents, request->Events, &events);

    fds[WiFiRequestHandler::POLL_FD_INDEX].fd = request->SocketHandle;
    fds[WiFiRequestHandler::POLL_FD_INDEX].events = (uint16_t) (events & 0xffff);
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd = dummy_socket_handle;
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].events = POLLIN;

    TRACE_MESSAGE("Poll: Requested Nuttx events 0x%04x maps to ESP events 0x%04x", request->Events, (uint) events);

    LockPollRequests();
    _pollRequests[request->SetupMessageId] = fds;
    UnlockPollRequests();

#if defined(WIFI_DEBUG)
    DumpActivePollRequests();
#endif

    SystemRequestHandler::SharedThreadPool()->Execute(PollSocket, reinterpret_cast<void *>(request->SetupMessageId));

    TRACE_MESSAGE("PollSetup: Exit, result %d", result);

    return (result);
}

/**
 *  @brief Teardown a poll request that has previously been setup by PollSetup.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollTeardown(void * const vpMessage)
{
    TRACE_MESSAGE("PollTeardown: Enter");
    int result = 0;

    Esp32Messaging::PollRequest * const request = (Esp32Messaging::PollRequest *) vpMessage;

#if defined(WIFI_DEBUG)
    DumpActivePollRequests();
#endif

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find(request->SetupMessageId);
    if (it != _pollRequests.end())
    {
        TRACE_MESSAGE("PollTeardown: Found socket handle for request 0x%08x", (uint) request->SetupMessageId);
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        result = write(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd, "E", 1); // Writing to the dummy file descriptor should end the poll request.
        if (close(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd) < 0)
        {
            result = -1;
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fds));
        _pollRequests.erase(it);
    }
    else
    {
        //
        //  We do not treat this as an error as the request information may have been removed by the
        //  interrupt handler.
        //
        TRACE_MESSAGE("PollTeardown: Cannot find request %08x", (uint) request->SetupMessageId);
    }
    UnlockPollRequests();

#if defined(WIFI_DEBUG)
    DumpActivePollRequests();
#endif

    TRACE_MESSAGE("PollTeardown: Exit, result %d", result);

    return (result);
}

/**
 *  @brief Execute the actual request to poll a socket.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::PollSocket(void * const requestId)
{
    TRACE_MESSAGE("PollSocket: Enter");

    Esp32Messaging::InterruptPollResponse response = {};

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find((uint32_t) requestId);
    UnlockPollRequests();
    if (it != _pollRequests.end())
    {
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        response.Result = poll(fds, 2, -1);
        if (response.Result < 0)
        {
            response.ResponseErrno = Mapping::NuttxErrno(errno);
        }
        else
        {
            LockPollRequests();
            it = _pollRequests.find((uint32_t) requestId); // Get again in case anything has changed.
            if (it != _pollRequests.end())
            {
                Mapping::GetStmFlagValue(Mapping::PollEvents, fds[POLL_FD_INDEX].revents, (int32_t *) &response.ReturnedEvents);
                if (response.ReturnedEvents != 0)
                {
                    response.Result = 1;
                }
                TRACE_MESSAGE("PollSocket: Returned ESP32 events 0x%04x maps to NuttX events 0x%04x", fds[POLL_FD_INDEX].revents, response.ReturnedEvents);
                response.SetupMessageId = (uint32_t) requestId;
                close(fds[POLL_DUMMY_FD_INDEX].fd);
                Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fds));
                _pollRequests.erase(it);
            }
            else
            {
                response.Result = -1;
                response.ResponseErrno = Mapping::NuttxErrno(EFAULT);
            }
            UnlockPollRequests();
        }
    }
    TRACE_MESSAGE("PollSocket: Return value from poll: %ld", response.Result);

    Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    if (message == NULL)
    {
        //
        //  There is not a lot we can do here - maybe consider a reset ???
        //
        ERROR_MESSAGE("PollSocket: Failed to allocate memory for message");
        return;
    }
    else
    {
        memset(static_cast<void *>(message), 0, sizeof(Message));
        message->Interface = Esp32Interfaces::WiFi;
        message->Function = WiFiFunction::InterruptPollResponse;
        message->MessageID = _messageDispatcher->GetNextMessageID();
        message->PayloadLength = Encoders::EncodedInterruptPollResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeInterruptPollResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        message->MessageType = MessageTypes::Event;
        _messageDispatcher->QueueMessageForStm32(message);
    }

    TRACE_MESSAGE("PollSocket: Exit");
}

/**
 *  @brief Expose the poll method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::Poll(void * const vpMessage)
{
    TRACE_MESSAGE("Poll: Enter");
    Message * const message = static_cast<Message *>(vpMessage);

    assert(message != NULL);
    assert(message->Payload != NULL);
    Esp32Messaging::IntegerAndErrnoResponse response = {};

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        Esp32Messaging::PollRequest *request = Encoders::ExtractPollRequest(message->Payload);
        if (request)
        {
            if (request->Setup == 1)
            {
                response.Result = PollSetup(static_cast<void *>(request));
            }
            else
            {
                response.Result = PollTeardown(static_cast<void *>(request));
            }

            if (response.Result < 0)
            {
                response.ResponseErrno = Mapping::NuttxErrno(errno);
            }
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));
        }
        else
        {
            result = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = Mapping::NuttxErrno(ENETDOWN);
    }

    message->DeletePayload();

    if (result != StatusCodes::CompletedOk)
    {
        message->StatusCode = result;
    }
    else
    {
        message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
    }
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Poll: Exit");
}
