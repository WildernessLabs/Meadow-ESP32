/**
 * @file PosixAPIHandlers.hpp
 * @author Mark Stevens
 * @brief Implement the POSIX API handlers for the WiFi service.
 * @date 2025-12-15
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include <inttypes.h>

#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"

/**
 * @brief Provide an interface to the POSIX API handlers for the WiFi service.
 */
class PosixApiHandlers
{
public:
    /**
     *  @brief Name of the component / task for the WiFi system.
     */
    static constexpr auto COMPONENT_NAME = "Posix";
    
    /**
     *  @brief Setup the Posix API handlers.
     */
    static void Setup();

    /**
     *  @brief Expose the POSIX getaddrinfo method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the getaddrinfo method.
     */
    static void GetAddrInfo(void * const vpMessage);

    /**
     *  @brief Expose the POSIX socket method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the socket method.
     */
    static void Socket(void * const vpMessage);

    /**
     *  @brief Expose the POSIX connect method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the connect method.
     */
    static void Connect(void * const vpMessage);

    /**
     *  @brief Expose the POSIX freeaddrinfo method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the freeaddrinfo method.
     */
    static void FreeAddrInfo(void * const vpMessage);

    /**
     *  @brief Expose the POSIX getsockopt method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the getsockopt method.
     */
    static void GetSockOpt(void * const vpMessage);

    /**
     *  @brief Expose the POSIX setsockopt method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the setsockopt method.
     */
    static void SetSockOpt(void * const vpMessage);

    /**
     *  @brief Expose the POSIX write method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the write method.
     */
    static void Write(void * const vpMessage);

    /**
     *  @brief Expose the POSIX read method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the read method.
     */
    static void Read(void * const vpMessage);

    /**
     *  @brief Expose the POSIX close method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the close method.
     */
    static void Close(void * const vpMessage);

    /**
     *  @brief Expose the POSIX send method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the send method.
     */
    static void Send(void * const vpMessage);

    /**
     *  @brief Expose the POSIX sendto method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the sendto method.
     */
    static void SendTo(void * const vpMessage);

    /**
     *  @brief Expose the POSIX recvfrom method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the recvfrom method.
     */
    static void RecvFrom(void * const vpMessage);

    /**
     *  @brief Expose the POSIX bind method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the bind method.
     */
    static void Bind(void * const vpMessage);

    /**
     *  @brief Expose the POSIX listen method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the listen method.
     */
    static void Listen(void * const vpMessage);

    /**
     *  @brief Expose the POSIX accept method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the accept method.
     */
    static void Accept(void * const vpMessage);

    /**
     *  @brief Expose the POSIX getsockname method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the getsockname method.
     */
    static void GetSockName(void * const vpMessage);

    /**
     *  @brief Expose the POSIX getpeername method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the getpeername method.
     */
    static void GetPeerName(void * const vpMessage);

    /**
     *  @brief Expose the poll method to the STM32.
     *
     *  @param message
     *      Message structure containing the parameters for the poll method.
     */
    static void Poll(void * const vpMessage);

    /**
     *  @brief Expose the simulation of the POSIX ioctl method to the STM32.
     *
     *  This is a simulation of the ioctl method as this is not supported by the
     *  ESP-IDF framework.
     *
     *  @param message
     *      Message structure containing the parameters for the ioctl method.
     */
    static void Ioctl(void * const vpMessage);

private:
    /**
     *  @brief Index of the dummy socket file descriptor in poll request.
     */
    static constexpr int POLL_DUMMY_FD_INDEX = 1;

    /**
     *  @brief Index of the actual socket file descriptor in poll request.
     */
    static constexpr int POLL_FD_INDEX = 0;

    /**
     *  @brief Mutex for the poll request linked list.
     */
    static SemaphoreHandle_t _pollRequestListMutex;

    /**
     *  @brief List of active poll requests.
     */
    static std::map<uint32_t, struct pollfd *> _pollRequests;

    /**
     *  @brief Delete default constructor, copy constructor, move constructor,
     *         copy assignment operator, and move assignment operator.
     */
    PosixApiHandlers() = delete;
    PosixApiHandlers(const PosixApiHandlers &) = delete;
    PosixApiHandlers(PosixApiHandlers &&) = delete;
    PosixApiHandlers &operator=(const PosixApiHandlers &) = delete;
    PosixApiHandlers &operator=(PosixApiHandlers &&) = delete;

    /**
     * @brief Get address information.
     *
     * @param request Request from the STM32
     * @param response Response to the STM32
     * @return StatusCodes::StatusCodes Result of this method, StatusCodes::CompletedOk if the GetAddrInfo operation succeeds,
     *         StatusCodes::Failed if the GetAddrInfo operation fails.
     */
    static StatusCodes::StatusCodes GetAddrInfo(Esp32Messaging::GetAddrInfoRequest * const request, Esp32Messaging::GetAddrInfoResponse * const response);

    /**
     * @brief Process the SO_SNDTIMEO and SO_SNDTIMEO get socket option.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param vpMessage Pointer to the request.
     */
    static inline StatusCodes::StatusCodes GetSendReceiveOption(const int socketHandle, const int level, const int optionName, Esp32Messaging::GetSockOptResponse &response);

    /**
     * @brief Process the SO_LINGER get socket option.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param vpMessage Pointer to the request.
     */
    static inline StatusCodes::StatusCodes GetLingerOption(const int socket, const int level, const int optionName, Esp32Messaging::GetSockOptResponse &response);

    /**
     * @brief Process the SO_RCVBUF get socket option.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param vpMessage Pointer to the request.
     */
    static inline StatusCodes::StatusCodes GetSendReceiveBuffer(const int socket, const int level, const int optionName, Esp32Messaging::GetSockOptResponse &response);

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
    static uint8_t *ExtractIntegerSocketOptionValue(const uint8_t * const data, socklen_t * const length);

    /**
     * @brief Setup the socket value and length for calls to SetSockOpt.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
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
    static inline uint8_t *GetSocketOptionValue(const int name, uint8_t * const optionValue, socklen_t * const length);

    /**
     * @brief Setup the TCP value and length for calls to SetSockOpt.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
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
    static inline uint8_t *GetTcpOptionValue(const int name, uint8_t * const optionValue, socklen_t * const length);

    /**
     * @brief Process the setsockopt request from the STM32.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param request Request from the STM32.
     * @param response Response to the STM32.
     *
     * @return StatusCodes::StatusCodes Status of the request.
     */
    static inline StatusCodes::StatusCodes SetSockOpt(const Esp32Messaging::SetSockOptRequest * const request, Esp32Messaging::IntegerAndErrnoResponse &response);

    /**
     * @brief Provide a mechanism for the STM32 to call the sendto method.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param request Request from the STM32.
     * @param response Response to the STM32.
     */
    static inline StatusCodes::StatusCodes SendTo(Esp32Messaging::SendToRequest *request, Esp32Messaging::IntegerAndErrnoResponse &response);

    /**
     * @brief Process the recvfrom request from the STM32.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param request Request from the STM32.
     * @param response Response to the STM32.
     * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
     *         StatusCodes::EspOutOfMemory if memory allocation fails.
     */
    static inline StatusCodes::StatusCodes RecvFrom(Esp32Messaging::RecvFromRequest *request, Esp32Messaging::RecvFromResponse &response);

    /**
     * @brief Bind a socket to an address.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param request Request from the STM32.
     * @param response Response to the STM32.
     * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
     *         StatusCodes::EspOutOfMemory if memory allocation fails.
     */
    static inline StatusCodes::StatusCodes Bind(Esp32Messaging::BindRequest *request, Esp32Messaging::IntegerAndErrnoResponse &response);

    /**
     * @brief Process the accept request from the STM32.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param request Request from the STM32.
     * @param response Response to the STM32.
     *
     * @return StatusCodes::StatusCodes Returns StatusCodes::CompletedOk if the operation succeeds,
     *         StatusCodes::EspOutOfMemory if memory allocation fails.
     */
    static inline StatusCodes::StatusCodes Accept(Esp32Messaging::AcceptRequest *request, Esp32Messaging::AcceptResponse &response);

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
    static StatusCodes::StatusCodes GetSockPeerName(const Esp32Messaging::GetSockPeerNameRequest * const request, const WiFiFunction::WiFiFunction methodToCall, Esp32Messaging::GetSockPeerNameResponse &response);

    /**
     * @brief Expose the POSIX getsockname method to the STM32.
     *
     * @param message
     *      Message structure containing the parameters for the getsockname method.
     *
     * @param methodToCall
     *      Which method should be called, getsockname or getpeername?
     */
    static void GetSockPeerName(void * const vpMessage, const WiFiFunction::WiFiFunction methodToCall);

    /**
     * @brief Lock the _pollRequests object.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     */
    static inline void LockPollRequests();

    /**
     * @brief Unlock the _pollRequests object.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     */
    static inline void UnlockPollRequests();

    /**
     * @brief Open a dummy socket to be used by the Poll methods to terminate a poll request.
     *
     * @returns
     *      Handle to the socket that has just been opened of -1 if there is a problem.
     */
    static inline int OpenDummySocket();

    /**
     *  @brief Debug method to dump all of the active poll requests.
     */
    static void DumpActivePollRequests();

    /**
     * @brief Setup a poll request an assign it to a thread in the thread pool.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param message
     *      Message structure containing the parameters for the poll method.
     */
    static inline int PollSetup(void * const vpMessage);

    /**
     * @brief Teardown a poll request that has previously been setup by PollSetup.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param message
     *      Message structure containing the parameters for the poll method.
     */
    static inline int PollTeardown(void * const vpMessage);

    /**
     * @brief Execute the actual request to poll a socket.
     * 
     * @note This method is only called by one method and so it is suggested that it is inlined.
     *
     * @param message
     *      Message structure containing the parameters for the poll method.
     */
    static inline void PollSocket(void * const requestId);
};
