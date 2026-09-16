/*
 *  test_posix.cpp
 *
 *  Implementation of the test methods for the POSIX socket API.
 *
 *  These tests drive the socket calls the way the STM32 does, by queueing messages for
 *  the WiFi request handler and reading the responses back off the outbound queue, so
 *  the encoding, the dispatch and the handlers are all exercised together.
 *
 *  The Encoders class only builds what the ESP32 sends and only reads what it
 *  receives, so there is nothing here to encode a request or decode a response.  The
 *  helpers below do that using the same primitives the encoders are built from, which
 *  also means these tests fail if the wire format changes underneath them.
 *
 *  Read and Write are deprecated and are deliberately not covered.  Sending uses Send,
 *  and receiving is tested through both RecvFrom and RecvMsg.
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"
#include "NvsManager.hpp"
#include "Utils.hpp"

#include "test_common.hpp"
#include "secrets.h"

//
//  These tests need a radio and a web server on the local network, so they are not
//  tagged [Emulated] and do not run under QEMU.
//
#define CLASS_OF_TESTS "[POSIX]"

//
//  The soak test runs for a long time and is left out of ordinary runs.  Select it with
//  the [Slow] tag when it is wanted.
//
#define SLOW_TESTS "[POSIX][Slow]"

//
//  The web server is described in secrets.h so its address does not end up in source
//  control.  WEB_SERVER_IP_ADDRESS and WEB_SERVER_PORT come from there; the page asked
//  for here is the server's default one.
//
//  secrets.h also defines SIMPLE_WEB_PAGE for a named resource, which a later test
//  fetching something other than the default page would use.
//
#define TEST_WEB_PAGE "/"

//
//  Content the default page is expected to contain.  Any server returning this markup
//  will do.
//
#define TEST_EXPECTED_HEADING "<h1>Simple Web Page</h1>"
#define TEST_EXPECTED_PARAGRAPH "<p>This is a small web page.</p>"

//
//  Sizes of the encoded requests, taken from the matching Extract methods in the
//  Encoders class.
//
#define ENCODED_SOCKET_REQUEST_SIZE 16
#define ENCODED_SOCK_ADDR_SIZE 31
#define ENCODED_CONNECT_REQUEST_SIZE (8 + ENCODED_SOCK_ADDR_SIZE)
#define ENCODED_RECV_FROM_REQUEST_SIZE 16
#define ENCODED_RECV_MSG_REQUEST_SIZE 20
#define ENCODED_CLOSE_REQUEST_SIZE 4

//
//  Address family as NuttX numbers it.  Connect maps this value through
//  Mapping::AddressFamily rather than using it directly.
//
#define NUTTX_AF_INET 2

//
//  Socket type and protocol are passed to socket() without being mapped, so these are
//  the values lwIP expects.
//
#define LWIP_SOCK_STREAM 1
#define LWIP_IPPROTO_TCP 6

//
//  Largest amount of data asked for in one call, and the most calls to make before
//  giving up on the response being complete.
//
#define RECEIVE_CHUNK_SIZE 512
#define MAXIMUM_RECEIVES 32

//
//  Time allowed for a socket operation to produce a response.
//
#define SOCKET_RESPONSE_TIMEOUT 10000

//
//  How long to wait between checks of the outbound queue.  A fetch makes six round
//  trips, so a coarse interval here dominates the time a fetch appears to take and
//  hides what the firmware is actually doing.
//
#define RESPONSE_POLL_INTERVAL 10

//
//  Number of times to try joining the access point before giving up.
//
#define CONNECTION_ATTEMPTS 3

//
//  Number of times the soak test fetches the page, and how often it reports progress.
//
#define SOAK_ITERATIONS 10000
#define SOAK_PROGRESS_INTERVAL 250

//
//  How far the free heap is allowed to fall across the whole soak test.  Allocation
//  patterns leave the heap slightly differently arranged run to run, so an exact match
//  is not expected, but a genuine leak over this many fetches is far larger than this.
//
#define SOAK_HEAP_ALLOWANCE 8192

/**
 * @brief Whether the access point has already been joined during this run.
 */
static bool _connectedToAccessPoint = false;

/**
 * @brief Response to a socket call that returns a result and an errno.
 */
struct SocketCallResult
{
    /**
     * @brief Value the underlying POSIX call returned.
     */
    int32_t Result;

    /**
     * @brief Error number reported, using the NuttX numbering.
     */
    int32_t ResponseErrno;
};

/**
 * @brief Queue a message for the ESP32 and return the response to it.
 *
 *  Connecting to an access point and the socket calls themselves generate event
 *  messages as well as responses, and the socket calls run on the shared thread pool
 *  so the response does not appear immediately.  This drains the outbound queue until
 *  the response to the given function arrives, discarding anything else.
 *
 * @param messageDispatcher Dispatcher whose outbound queue is being read.
 * @param function Function the response is expected to carry.
 * @param timeout Milliseconds to wait for the response.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
static Message *WaitForResponseTo(MockMessageDispatcher *messageDispatcher, const WiFiFunction::WiFiFunction function, const unsigned timeout)
{
    unsigned waited = 0;

    while (waited < timeout)
    {
        if (messageDispatcher->OutboundMessageQueueLength() == 0)
        {
            vTaskDelay(RESPONSE_POLL_INTERVAL / portTICK_PERIOD_MS);
            waited += RESPONSE_POLL_INTERVAL;
            continue;
        }

        Message *message = messageDispatcher->GetOutboundMessage();
        if (message == nullptr)
        {
            continue;
        }

        if ((message->MessageType == MessageTypes::Response) && (message->Function == function))
        {
            return (message);
        }

        //
        //  An event, or the response to something else, so it is of no interest here.
        //
        Message::DeleteMessage(message);
    }

    return (nullptr);
}

/**
 * @brief Queue a request and return the response it produced.
 *
 * @param messageDispatcher Dispatcher to queue the request on.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
static Message *Call(MockMessageDispatcher *messageDispatcher, const WiFiFunction::WiFiFunction function, uint8_t *payload, const uint32_t payloadLength)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, function, StatusCodes::Failure, 0xff, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(request);
    messageDispatcher->QueueMessageForEsp32(request);

    return (WaitForResponseTo(messageDispatcher, function, SOCKET_RESPONSE_TIMEOUT));
}

/**
 * @brief Queue a request and read back the result and errno it produced.
 *
 * @param messageDispatcher Dispatcher to queue the request on.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param callResult Filled in with the result and errno from the response.
 *
 * @return true if a response arrived, false if the call timed out.
 */
static bool CallReturningResultAndErrno(MockMessageDispatcher *messageDispatcher, const WiFiFunction::WiFiFunction function, uint8_t *payload, const uint32_t payloadLength, SocketCallResult *callResult)
{
    Message *response = Call(messageDispatcher, function, payload, payloadLength);
    if (response == nullptr)
    {
        return (false);
    }

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_NOT_NULL(response->Payload);

    callResult->Result = Encoders::ExtractInt32(response->Payload);
    callResult->ResponseErrno = Encoders::ExtractInt32(response->Payload + 4);

    Message::DeleteMessage(response);

    return (true);
}

/**
 * @brief Encode a socket request.
 *
 * @param domain Address family for the socket.
 * @param type Type of socket required.
 * @param protocol Protocol the socket should use.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeSocketRequest(const int32_t domain, const int32_t type, const int32_t protocol)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_SOCKET_REQUEST_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, ENCODED_SOCKET_REQUEST_SIZE);

    Encoders::EncodeUInt32(0, buffer); // AddressInformation is not used here.
    Encoders::EncodeInt32(domain, buffer + 4);
    Encoders::EncodeInt32(type, buffer + 8);
    Encoders::EncodeInt32(protocol, buffer + 12);

    return (buffer);
}

/**
 * @brief Encode a connect request for an IPv4 address.
 *
 *  The handler copies the port and the address into a sockaddr_in without converting
 *  them, so both are encoded in network byte order.
 *
 * @param socketHandle Socket to connect.
 * @param address Dotted quad address of the server.
 * @param port Port to connect to, in host byte order.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeConnectRequest(const int32_t socketHandle, const char *address, const uint16_t port)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_CONNECT_REQUEST_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
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
 * @brief Encode a send request.
 *
 * @param socketHandle Socket to send on.
 * @param data Data to be sent.
 * @param length Number of bytes to send.
 * @param encodedLength Filled in with the length of the encoded request.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeSendRequest(const int32_t socketHandle, const char *data, const uint32_t length, uint32_t *encodedLength)
{
    *encodedLength = 4 + 4 + length + 4 + 4;
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(*encodedLength));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, *encodedLength);

    Encoders::EncodeInt32(socketHandle, buffer);
    Encoders::EncodeUInt32(length, buffer + 4);
    memcpy(buffer + 8, data, length);
    Encoders::EncodeInt32(static_cast<int32_t>(length), buffer + 8 + length);
    Encoders::EncodeInt32(0, buffer + 12 + length); // No flags.

    return (buffer);
}

/**
 * @brief Encode a recvfrom request.
 *
 * @param socketHandle Socket to receive from.
 * @param length Largest number of bytes wanted.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeRecvFromRequest(const int32_t socketHandle, const int32_t length)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_RECV_FROM_REQUEST_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, ENCODED_RECV_FROM_REQUEST_SIZE);

    Encoders::EncodeInt32(socketHandle, buffer);
    Encoders::EncodeInt32(length, buffer + 4);
    Encoders::EncodeInt32(0, buffer + 8);  // No flags.
    Encoders::EncodeInt32(1, buffer + 12); // Ask for the source address.

    return (buffer);
}

/**
 * @brief Encode a recvmsg request.
 *
 * @param socketHandle Socket to receive from.
 * @param length Largest number of bytes wanted.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeRecvMsgRequest(const int32_t socketHandle, const int32_t length)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_RECV_MSG_REQUEST_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, ENCODED_RECV_MSG_REQUEST_SIZE);

    Encoders::EncodeInt32(socketHandle, buffer);
    Encoders::EncodeInt32(length, buffer + 4);
    Encoders::EncodeInt32(0, buffer + 8);  // No flags.
    Encoders::EncodeInt32(1, buffer + 12); // Ask for the source address.
    Encoders::EncodeUInt32(0, buffer + 16); // No control data wanted.

    return (buffer);
}

/**
 * @brief Encode a close request.
 *
 * @param socketHandle Socket to close.
 *
 * @return Pointer to the encoded request, which the caller owns.
 */
static uint8_t *EncodeCloseRequest(const int32_t socketHandle)
{
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(ENCODED_CLOSE_REQUEST_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);

    Encoders::EncodeInt32(socketHandle, buffer);

    return (buffer);
}

/**
 * @brief Connect to the access point named in secrets.h.
 *
 *  Socket creation is refused while the interface is down, so the tests cannot do
 *  anything until this has succeeded.
 *
 * @param messageDispatcher Dispatcher to queue the request on.
 *
 * @return true if the connection was made.
 */
static bool ConnectToTestAccessPoint(MockMessageDispatcher *messageDispatcher)
{
    //
    //  The tests share one dispatcher and therefore one WiFi request handler, so the
    //  interface only has to be brought up once however many of them run.  The handler
    //  keeps this state itself but does not expose it, so it is remembered here rather
    //  than widening the handler's interface for the sake of a test.
    //
    if (_connectedToAccessPoint)
    {
        return (true);
    }

    Esp32Messaging::AccessPointInformation accessPointInformation = {};
    memset(&accessPointInformation, 0, sizeof(accessPointInformation));
    accessPointInformation.NetworkName = (char *) WIFI_NETWORK;  // cppcheck-suppress cstyleCast
    accessPointInformation.Password = (char *) WIFI_PASSWORD;    // cppcheck-suppress cstyleCast

    uint32_t payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&accessPointInformation);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 0, payloadLength);
    Encoders::EncodeAccessPointInformation(&accessPointInformation, payload);

    //
    //  The first association after a reset regularly fails, the radio going from assoc
    //  back to init, and the next attempt then succeeds.  Automatic reconnection is not
    //  turned on through the message interface, so the retry is done here.
    //
    const unsigned timeout = WiFiRequestHandler::GetWiFiTimeout() + 10000;

    for (int attempt = 1; (attempt <= CONNECTION_ATTEMPTS) && !_connectedToAccessPoint; attempt++)
    {
        printf("Connecting to access point %s, attempt %d.\n", WIFI_NETWORK, attempt);

        uint8_t *attemptPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        TEST_ASSERT_NOT_NULL(attemptPayload);
        memcpy(attemptPayload, payload, payloadLength);

        Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, WiFiFunction::ConnectToAccessPoint, StatusCodes::Failure, 0xff, attemptPayload, payloadLength);
        TEST_ASSERT_NOT_NULL(request);
        messageDispatcher->QueueMessageForEsp32(request);

        Message *response = WaitForResponseTo(messageDispatcher, WiFiFunction::ConnectToAccessPoint, timeout);
        if (response != nullptr)
        {
            _connectedToAccessPoint = (response->StatusCode == StatusCodes::CompletedOk);
            Message::DeleteMessage(response);
        }
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&payload));

    return (_connectedToAccessPoint);
}

/**
 * @brief Open a socket to the test web server and send a request for the default page.
 *
 * @param messageDispatcher Dispatcher to queue the requests on.
 *
 * @return Handle of the connected socket.
 */
static int32_t SendPageRequest(MockMessageDispatcher *messageDispatcher)
{
    SocketCallResult callResult = {};

    uint8_t *payload = EncodeSocketRequest(NUTTX_AF_INET, LWIP_SOCK_STREAM, LWIP_IPPROTO_TCP);
    if (!CallReturningResultAndErrno(messageDispatcher, WiFiFunction::Socket, payload, ENCODED_SOCKET_REQUEST_SIZE, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the socket request.");
    }
    if (callResult.Result < 0)
    {
        printf("socket() failed, errno %" PRIi32 "\n", callResult.ResponseErrno);
        TEST_FAIL_MESSAGE("Could not create the socket.");
    }
    const int32_t socketHandle = callResult.Result;
    printf("Socket %" PRIi32 " created.\n", socketHandle);

    payload = EncodeConnectRequest(socketHandle, WEB_SERVER_IP_ADDRESS, WEB_SERVER_PORT);
    if (!CallReturningResultAndErrno(messageDispatcher, WiFiFunction::Connect, payload, ENCODED_CONNECT_REQUEST_SIZE, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the connect request.");
    }
    if (callResult.Result < 0)
    {
        printf("connect() failed, errno %" PRIi32 "\n", callResult.ResponseErrno);
        TEST_FAIL_MESSAGE("Could not connect to the web server.");
    }
    printf("Connected to %s:%d.\n", WEB_SERVER_IP_ADDRESS, (int) WEB_SERVER_PORT);

    //
    //  Closing the connection afterwards is requested so the server ends the response
    //  rather than leaving the receive waiting for more.
    //
    char httpRequest[256];
    if (WEB_SERVER_PORT == 80)
    {
        snprintf(httpRequest, sizeof(httpRequest),
                 "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                 TEST_WEB_PAGE, WEB_SERVER_IP_ADDRESS);
    }
    else
    {
        snprintf(httpRequest, sizeof(httpRequest),
                 "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
                 TEST_WEB_PAGE, WEB_SERVER_IP_ADDRESS, (int) WEB_SERVER_PORT);
    }

    uint32_t requestLength = strlen(httpRequest);
    uint32_t encodedLength = 0;
    payload = EncodeSendRequest(socketHandle, httpRequest, requestLength, &encodedLength);
    if (!CallReturningResultAndErrno(messageDispatcher, WiFiFunction::Send, payload, encodedLength, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the send request.");
    }
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(requestLength), callResult.Result);

    return (socketHandle);
}

/**
 * @brief Close a socket and check the call succeeded.
 *
 * @param messageDispatcher Dispatcher to queue the request on.
 * @param socketHandle Socket to close.
 */
static void CloseSocket(MockMessageDispatcher *messageDispatcher, const int32_t socketHandle)
{
    SocketCallResult callResult = {};
    uint8_t *payload = EncodeCloseRequest(socketHandle);

    if (!CallReturningResultAndErrno(messageDispatcher, WiFiFunction::Close, payload, ENCODED_CLOSE_REQUEST_SIZE, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the close request.");
    }
    TEST_ASSERT_EQUAL_INT32(0, callResult.Result);
}

/**
 * @brief Check the page that was fetched is the one the server was expected to return.
 *
 * @param page Everything received from the server.
 * @param complete Whether the server closed the connection.
 */
static void CheckPageContents(const std::string &page, const bool complete)
{
    printf("Received %u bytes from the server.\n", (unsigned) page.length());

    TEST_ASSERT_TRUE_MESSAGE(complete, "The server did not close the connection.");
    TEST_ASSERT_TRUE_MESSAGE(page.find("200 OK") != std::string::npos, "The server did not return 200 OK.");
    TEST_ASSERT_TRUE_MESSAGE(page.find(TEST_EXPECTED_HEADING) != std::string::npos, "The page did not contain the expected heading.");
    TEST_ASSERT_TRUE_MESSAGE(page.find(TEST_EXPECTED_PARAGRAPH) != std::string::npos, "The page did not contain the expected paragraph.");
}

/**
 * @brief Fetch the default page from the test web server once.
 *
 *  Runs the whole sequence the STM32 would: create a socket, connect it to the server,
 *  send an HTTP request, receive until the server closes the connection, and close the
 *  socket.  Failures are reported here rather than asserted so that a caller running
 *  this repeatedly can say which attempt went wrong.
 *
 * @param messageDispatcher Dispatcher to queue the requests on.
 * @param receiveFunction Receive call to use, either RecvFrom or RecvMsg.
 * @param page Filled in with everything the server sent.
 *
 * @return true if the page was fetched and the server closed the connection.
 */
static bool FetchPageOnce(MockMessageDispatcher *messageDispatcher, const WiFiFunction::WiFiFunction receiveFunction, std::string *page)
{
    const int32_t socketHandle = SendPageRequest(messageDispatcher);
    const uint32_t requestSize = (receiveFunction == WiFiFunction::RecvMsg) ? ENCODED_RECV_MSG_REQUEST_SIZE : ENCODED_RECV_FROM_REQUEST_SIZE;

    bool complete = false;
    bool failed = false;

    for (int receives = 0; (receives < MAXIMUM_RECEIVES) && !complete && !failed; receives++)
    {
        uint8_t *payload = (receiveFunction == WiFiFunction::RecvMsg)
                               ? EncodeRecvMsgRequest(socketHandle, RECEIVE_CHUNK_SIZE)
                               : EncodeRecvFromRequest(socketHandle, RECEIVE_CHUNK_SIZE);

        Message *response = Call(messageDispatcher, receiveFunction, payload, requestSize);
        if (response == nullptr)
        {
            printf("No response to the receive request.\n");
            failed = true;
            break;
        }

        if ((response->StatusCode != StatusCodes::CompletedOk) || (response->Payload == nullptr))
        {
            printf("Receive returned status %d.\n", (int) response->StatusCode);
            Message::DeleteMessage(response);
            failed = true;
            break;
        }

        const uint32_t bufferLength = Encoders::ExtractUInt32(response->Payload);
        const int32_t result = Encoders::ExtractInt32(response->Payload + 4 + bufferLength);
        const int32_t responseErrno = Encoders::ExtractInt32(response->Payload + 8 + bufferLength);

        if (result < 0)
        {
            printf("Receive failed, errno %" PRIi32 "\n", responseErrno);
            Message::DeleteMessage(response);
            failed = true;
            break;
        }

        if (result > 0)
        {
            page->append(reinterpret_cast<const char *>(response->Payload + 4), result);
        }

        //
        //  A receive of zero bytes means the server has closed the connection.
        //
        complete = (result == 0);
        Message::DeleteMessage(response);
    }

    CloseSocket(messageDispatcher, socketHandle);

    return (complete && !failed);
}

/**
 * @brief Fetch the default page from the test web server using recvfrom.
 */
TEST_CASE("Fetch a web page using recvfrom", CLASS_OF_TESTS)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    if (!ConnectToTestAccessPoint(messageDispatcher))
    {
        TEST_FAIL_MESSAGE("Could not connect to the access point named in secrets.h.");
    }

    std::string page;
    const bool complete = FetchPageOnce(messageDispatcher, WiFiFunction::RecvFrom, &page);

    CheckPageContents(page, complete);
}

/**
 * @brief Fetch the default page from the test web server using recvmsg.
 *
 *  The same sequence as the recvfrom test, so that the two receive paths are compared
 *  against the same server and the same page.
 */
TEST_CASE("Fetch a web page using recvmsg", CLASS_OF_TESTS)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    if (!ConnectToTestAccessPoint(messageDispatcher))
    {
        TEST_FAIL_MESSAGE("Could not connect to the access point named in secrets.h.");
    }

    std::string page;
    const bool complete = FetchPageOnce(messageDispatcher, WiFiFunction::RecvMsg, &page);

    CheckPageContents(page, complete);
}

/**
 * @brief Fetch the page repeatedly to look for leaks and handle exhaustion.
 *
 *  Each iteration opens a socket, fetches the page and closes the socket again, so
 *  anything the socket path fails to release accumulates.  A leak of only a few bytes
 *  per fetch is invisible in a single run but obvious over thousands, and a socket
 *  handle that is not returned shows up as socket creation starting to fail.
 *
 *  The two receive paths are alternated so both are covered, and the free heap is
 *  reported as the run progresses so a slow leak can be seen developing rather than
 *  only being caught by the check at the end.
 *
 *  This takes a long time and needs the web server, so it is tagged [Slow] and is left
 *  out of ordinary runs.
 */
TEST_CASE("Fetch a web page repeatedly", SLOW_TESTS)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    if (!ConnectToTestAccessPoint(messageDispatcher))
    {
        TEST_FAIL_MESSAGE("Could not connect to the access point named in secrets.h.");
    }

    //
    //  One fetch before the measurements start, so that anything allocated once and
    //  kept is not counted as a leak.
    //
    std::string warmUp;
    if (!FetchPageOnce(messageDispatcher, WiFiFunction::RecvFrom, &warmUp))
    {
        TEST_FAIL_MESSAGE("The first fetch failed, so there is nothing to soak test.");
    }

    const uint32_t heapAtStart = esp_get_free_heap_size();
    const int64_t startedAt = esp_timer_get_time();

    printf("Fetching the page %d times, free heap %" PRIu32 " bytes.\n", SOAK_ITERATIONS, heapAtStart);

    for (int iteration = 1; iteration <= SOAK_ITERATIONS; iteration++)
    {
        const WiFiFunction::WiFiFunction receiveFunction = ((iteration % 2) == 0) ? WiFiFunction::RecvMsg : WiFiFunction::RecvFrom;

        std::string page;
        if (!FetchPageOnce(messageDispatcher, receiveFunction, &page))
        {
            printf("Fetch %d of %d failed using %s.\n", iteration, SOAK_ITERATIONS,
                   (receiveFunction == WiFiFunction::RecvMsg) ? "recvmsg" : "recvfrom");
            TEST_FAIL_MESSAGE("A fetch failed during the soak test.");
        }

        if (page.find(TEST_EXPECTED_HEADING) == std::string::npos)
        {
            printf("Fetch %d of %d returned %u bytes without the expected content.\n", iteration, SOAK_ITERATIONS, (unsigned) page.length());
            TEST_FAIL_MESSAGE("A fetch returned the wrong content during the soak test.");
        }

        if ((iteration % SOAK_PROGRESS_INTERVAL) == 0)
        {
            const int64_t elapsed = esp_timer_get_time() - startedAt;
            printf("  %d fetches, free heap %" PRIu32 " bytes, lowest %" PRIu32 ", %" PRId64 " ms per fetch.\n",
                   iteration, esp_get_free_heap_size(), esp_get_minimum_free_heap_size(),
                   (elapsed / 1000) / iteration);
        }
    }

    const uint32_t heapAtEnd = esp_get_free_heap_size();
    const int64_t elapsed = esp_timer_get_time() - startedAt;

    printf("Completed %d fetches in %" PRId64 " seconds.\n", SOAK_ITERATIONS, elapsed / 1000000);
    printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end.\n", heapAtStart, heapAtEnd);

    //
    //  The heap is not expected to come back to exactly where it started, because
    //  allocation patterns leave it slightly differently arranged, but a real leak over
    //  this many iterations is far larger than the allowance below.
    //
    if (heapAtEnd < heapAtStart)
    {
        const uint32_t lost = heapAtStart - heapAtEnd;
        printf("Free heap fell by %" PRIu32 " bytes over %d fetches.\n", lost, SOAK_ITERATIONS);
        TEST_ASSERT_TRUE_MESSAGE(lost <= SOAK_HEAP_ALLOWANCE, "The free heap fell too far, which suggests a leak in the socket path.");
    }
}
