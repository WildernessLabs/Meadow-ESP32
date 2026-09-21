/*
 *  TestPosix.cpp
 *
 *  Implementation of the test methods for the POSIX socket API.
 *
 *  These tests drive the socket calls the way the STM32 does, by queueing messages for
 *  the WiFi request handler and reading the responses back off the outbound queue, so
 *  the encoding, the dispatch and the handlers are all exercised together.
 *
 *  The Encoders class only builds what the ESP32 sends and only reads what it
 *  receives, so there is nothing there to encode a request or decode a response.  The
 *  TestEncoders namespace does that using the same primitives the encoders are built
 *  from, which also means these tests fail if the wire format changes underneath them.
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

#include "TestEncoders.hpp"
#include "TestHarness.hpp"
#include "UnitTestConfiguration.hpp"
#include "TestWiFiHelpers.hpp"

//
//  These tests need a radio and a web server on the local network, so they are not
//  tagged [Emulated] and do not run under QEMU.
//
#define CLASS_OF_TESTS "[POSIX][HardwareOnly]"

//
//  The soak test runs for a long time and is left out of ordinary runs.  Select it with
//  the [Slow] tag when it is wanted.
//
#define SLOW_TESTS "[POSIX][HardwareOnly][Slow]"

//
//  The web server is described in the configuration file on the device, so its address
//  does not end up in source control.  The address and the port come from there; the page
//  asked for here is the server's default one.
//
//  The configuration also carries SimpleWebPage for a named resource, which a later test
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

/**
 * @brief Make a socket call and read back the result and errno it produced.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Socket function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param callResult Filled in with the result and errno from the response.
 *
 * @return True if a response arrived, false if the call timed out.
 */
static bool SocketCall(MockMessageDispatcher *dispatcher, const WiFiFunction::WiFiFunction function, uint8_t *payload, const uint32_t payloadLength, SocketCallResult *callResult)
{
    return (SendRequestForResultAndErrno(dispatcher, Esp32Interfaces::WiFi, function, payload, payloadLength, SOCKET_RESPONSE_TIMEOUT, callResult));
}

/**
 * @brief Open a socket to the test web server and send a request for the default page.
 *
 * @param dispatcher Dispatcher to queue the requests on.
 *
 * @return Handle of the connected socket.
 */
static int32_t SendPageRequest(MockMessageDispatcher *dispatcher)
{
    SocketCallResult callResult = {};

    uint8_t *payload = TestEncoders::EncodeSocketRequest(TestEncoders::NUTTX_AF_INET, LWIP_SOCK_STREAM, LWIP_IPPROTO_TCP);
    if (!SocketCall(dispatcher, WiFiFunction::Socket, payload, TestEncoders::ENCODED_SOCKET_REQUEST_SIZE, &callResult))
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

    payload = TestEncoders::EncodeConnectRequest(socketHandle, UnitTestConfiguration::WebServerIpAddress(), UnitTestConfiguration::WebServerPort());
    if (!SocketCall(dispatcher, WiFiFunction::Connect, payload, TestEncoders::ENCODED_CONNECT_REQUEST_SIZE, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the connect request.");
    }
    if (callResult.Result < 0)
    {
        printf("connect() failed, errno %" PRIi32 "\n", callResult.ResponseErrno);
        TEST_FAIL_MESSAGE("Could not connect to the web server.");
    }
    printf("Connected to %s:%" PRIu32 ".\n", UnitTestConfiguration::WebServerIpAddress(), UnitTestConfiguration::WebServerPort());

    //
    //  Closing the connection afterwards is requested so the server ends the response
    //  rather than leaving the receive waiting for more.
    //
    char httpRequest[256];
    if (UnitTestConfiguration::WebServerPort() == 80)
    {
        snprintf(httpRequest, sizeof(httpRequest), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", TEST_WEB_PAGE, UnitTestConfiguration::WebServerIpAddress());
    }
    else
    {
        snprintf(httpRequest, sizeof(httpRequest), "GET %s HTTP/1.1\r\nHost: %s:%" PRIu32 "\r\nConnection: close\r\n\r\n", TEST_WEB_PAGE, UnitTestConfiguration::WebServerIpAddress(), UnitTestConfiguration::WebServerPort());
    }

    uint32_t requestLength = strlen(httpRequest);
    uint32_t encodedLength = 0;
    payload = TestEncoders::EncodeSendRequest(socketHandle, httpRequest, requestLength, &encodedLength);
    if (!SocketCall(dispatcher, WiFiFunction::Send, payload, encodedLength, &callResult))
    {
        TEST_FAIL_MESSAGE("No response to the send request.");
    }
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(requestLength), callResult.Result);

    return (socketHandle);
}

/**
 * @brief Close a socket and check the call succeeded.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param socketHandle Socket to close.
 */
static void CloseSocket(MockMessageDispatcher *dispatcher, const int32_t socketHandle)
{
    SocketCallResult callResult = {};
    uint8_t *payload = TestEncoders::EncodeCloseRequest(socketHandle);

    if (!SocketCall(dispatcher, WiFiFunction::Close, payload, TestEncoders::ENCODED_CLOSE_REQUEST_SIZE, &callResult))
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
    printf("Received %u bytes from the server.\n", static_cast<unsigned>(page.length()));

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
 * @param dispatcher Dispatcher to queue the requests on.
 * @param receiveFunction Receive call to use, either RecvFrom or RecvMsg.
 * @param page Filled in with everything the server sent.
 *
 * @return True if the page was fetched and the server closed the connection.
 */
static bool FetchPageOnce(MockMessageDispatcher *dispatcher, const WiFiFunction::WiFiFunction receiveFunction, std::string *page)
{
    const int32_t socketHandle = SendPageRequest(dispatcher);
    const uint32_t requestSize = (receiveFunction == WiFiFunction::RecvMsg) ? TestEncoders::ENCODED_RECV_MSG_REQUEST_SIZE : TestEncoders::ENCODED_RECV_FROM_REQUEST_SIZE;

    bool complete = false;
    bool failed = false;

    for (int receives = 0; (receives < MAXIMUM_RECEIVES) && !complete && !failed; receives++)
    {
        uint8_t *payload = (receiveFunction == WiFiFunction::RecvMsg) ? TestEncoders::EncodeRecvMsgRequest(socketHandle, RECEIVE_CHUNK_SIZE) : TestEncoders::EncodeRecvFromRequest(socketHandle, RECEIVE_CHUNK_SIZE);

        Message *response = SendRequest(dispatcher, Esp32Interfaces::WiFi, receiveFunction, payload, requestSize, SOCKET_RESPONSE_TIMEOUT);
        if (response == nullptr)
        {
            printf("No response to the receive request.\n");
            failed = true;
            break;
        }

        if ((response->StatusCode != StatusCodes::CompletedOk) || (response->Payload == nullptr))
        {
            printf("Receive returned status %d.\n", static_cast<int>(response->StatusCode));
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

    CloseSocket(dispatcher, socketHandle);

    return (complete && !failed);
}

/**
 * @brief Create the dispatcher and join the access point named in the configuration.
 *
 * @return Pointer to the dispatcher the tests should use.
 */
static MockMessageDispatcher *SetupConnectedDispatcher()
{
    NvsManager::Setup();

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    if (!ConnectToTestAccessPoint(dispatcher))
    {
        TEST_FAIL_MESSAGE("Could not connect to the access point named in the configuration.");
    }

    return (dispatcher);
}

/**
 * @brief Check that FreeAddrInfo refuses a handle it never issued, rather than trusting
 *        whatever value the STM32 sends back.
 *
 *  Regression check for issue #220.  GetAddrInfo now hands the STM32 an opaque handle
 *  instead of the real heap address of the addrinfo chain, and FreeAddrInfo has to
 *  refuse a handle it is not currently holding - never issued, already freed, or
 *  replayed from before an ESP32 restart - rather than calling freeaddrinfo on whatever
 *  address that value happens to decode to.
 *
 * @note This does not need a DNS lookup or a live connection, only a WiFi request
 *       handler, but sending it any WiFi-interface message brings the handler up the same
 *       way every other test in this file does - through MockMessageDispatcher, which
 *       creates it and calls Setup() the first time it sees one.  Setup() touches the
 *       radio and the antenna GPIOs, so this is CLASS_OF_TESTS like the rest of the file
 *       rather than [Emulated].
 */
TEST_CASE("FreeAddrInfo refuses a handle it never issued", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(sizeof(uint32_t)));
    TEST_ASSERT_NOT_NULL(payload);
    Encoders::EncodeUInt32(0xdeadbeef, payload);

    Message *response = SendRequest(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::FreeAddrInfo, payload, sizeof(uint32_t), SOCKET_RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The FreeAddrInfo request was never answered.");

    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, response->StatusCode);

    Message::DeleteMessage(response);
}

/**
 * @brief Fetch the default page from the test web server using recvfrom.
 */
TEST_CASE("Fetch a web page using recvfrom", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupConnectedDispatcher();

    std::string page;
    const bool complete = FetchPageOnce(dispatcher, WiFiFunction::RecvFrom, &page);

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
    MockMessageDispatcher *dispatcher = SetupConnectedDispatcher();

    std::string page;
    const bool complete = FetchPageOnce(dispatcher, WiFiFunction::RecvMsg, &page);

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
    MockMessageDispatcher *dispatcher = SetupConnectedDispatcher();

    //
    //  One fetch before the measurements start, so that anything allocated once and
    //  kept is not counted as a leak.
    //
    std::string warmUp;
    if (!FetchPageOnce(dispatcher, WiFiFunction::RecvFrom, &warmUp))
    {
        TEST_FAIL_MESSAGE("The first fetch failed, so there is nothing to soak test.");
    }

    const uint32_t iterations = UnitTestConfiguration::PosixIterations();
    const uint32_t progressInterval = UnitTestConfiguration::SoakProgressInterval();
    const uint32_t heapAtStart = esp_get_free_heap_size();
    const int64_t startedAt = esp_timer_get_time();

    printf("Fetching the page %" PRIu32 " times, free heap %" PRIu32 " bytes.\n", iterations, heapAtStart);

    for (uint32_t iteration = 1; iteration <= iterations; iteration++)
    {
        const WiFiFunction::WiFiFunction receiveFunction = ((iteration % 2) == 0) ? WiFiFunction::RecvMsg : WiFiFunction::RecvFrom;

        std::string page;
        if (!FetchPageOnce(dispatcher, receiveFunction, &page))
        {
            printf("Fetch %" PRIu32 " of %" PRIu32 " failed using %s.\n", iteration, iterations, (receiveFunction == WiFiFunction::RecvMsg) ? "recvmsg" : "recvfrom");
            TEST_FAIL_MESSAGE("A fetch failed during the soak test.");
        }

        if (page.find(TEST_EXPECTED_HEADING) == std::string::npos)
        {
            printf("Fetch %" PRIu32 " of %" PRIu32 " returned %u bytes without the expected content.\n", iteration, iterations, static_cast<unsigned>(page.length()));
            TEST_FAIL_MESSAGE("A fetch returned the wrong content during the soak test.");
        }

        if ((progressInterval > 0) && ((iteration % progressInterval) == 0))
        {
            const int64_t elapsed = esp_timer_get_time() - startedAt;
            printf("  %" PRIu32 " fetches, free heap %" PRIu32 " bytes, lowest %" PRIu32 ", %" PRId64 " ms per fetch.\n", iteration, esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), (elapsed / 1000) / iteration);
        }
    }

    const uint32_t heapAtEnd = esp_get_free_heap_size();
    const int64_t elapsed = esp_timer_get_time() - startedAt;

    printf("Completed %" PRIu32 " fetches in %" PRId64 " seconds.\n", iterations, elapsed / 1000000);
    printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end.\n", heapAtStart, heapAtEnd);

    //
    //  The heap is not expected to come back to exactly where it started, because
    //  allocation patterns leave it slightly differently arranged, but a real leak over
    //  this many fetches is far larger than the allowance the configuration gives.
    //
    if (heapAtEnd < heapAtStart)
    {
        const uint32_t allowance = UnitTestConfiguration::SoakHeapAllowance();
        const uint32_t lost = heapAtStart - heapAtEnd;
        printf("Free heap fell by %" PRIu32 " bytes over %" PRIu32 " fetches, against an allowance of %" PRIu32 ".\n", lost, iterations, allowance);
        TEST_ASSERT_TRUE_MESSAGE(lost <= allowance, "The free heap fell too far, which suggests a leak in the socket path.");
    }
}
