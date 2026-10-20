/*
 *  TestPollTeardown.cpp
 *
 *  Tests that a poll request has one owner at a time: that tearing one down wakes the worker blocked in
 *  poll(), waits for it to leave, and only then closes the wake socket and frees the request (issues #239
 *  and #395).
 *
 *  The worker is run in a task of the test's own rather than on the shared thread pool, whose semaphore is
 *  shared with the ThreadPool tests.  The sockets are lwIP's loopback, so no WiFi connection is needed.
 */

#include "sdkconfig.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <stdint.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "WiFiRequestHandler.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[PollTeardown][Emulated]"

/**
 * @brief Milliseconds given to a worker to reach poll() and block in it.
 */
#define WORKER_SETTLING_TIME 200

/**
 * @brief Milliseconds to wait for a worker task to finish.
 */
#define WORKER_TIMEOUT 3000

/**
 * @brief What the task running a worker is given.
 */
struct WorkerContext
{
    /**
     * @brief The request to poll.
     */
    void *ActivePollPointer;

    /**
     * @brief Given by the task when PollSocket has returned.
     */
    SemaphoreHandle_t Done;
};

/**
 * @brief Gives the tests access to WiFiRequestHandler's poll requests.
 */
class TestPollTeardown
{
public:
    /**
     * @brief A poll request, a type that only the friends of WiFiRequestHandler can name.
     */
    typedef WiFiRequestHandler::ActivePoll Poll;

    /**
     * @brief Lock the list of poll requests, as the handler does.
     */
    static void Lock()
    {
        xSemaphoreTake(WiFiRequestHandler::_pollRequestListMutex, portMAX_DELAY);
    }

    /**
     * @brief Unlock the list of poll requests.
     */
    static void Unlock()
    {
        xSemaphoreGive(WiFiRequestHandler::_pollRequestListMutex);
    }

    /**
     * @brief Make the sockets usable and give the handler a dispatcher the events can be read from.
     *
     * @return The dispatcher, which stays in place until Finish().
     */
    static MockMessageDispatcher *Start()
    {
        TEST_ASSERT_EQUAL(ESP_OK, esp_netif_init());
        //
        //  The handler makes the list's mutex when it is constructed, and these tests may run without one.
        //
        if (WiFiRequestHandler::_pollRequestListMutex == nullptr)
        {
            WiFiRequestHandler::_pollRequestListMutex = xSemaphoreCreateMutex();
            TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_pollRequestListMutex);
        }
        _savedDispatcher = WiFiRequestHandler::_messageDispatcher;
        _dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(_dispatcher);
        WiFiRequestHandler::_messageDispatcher = _dispatcher;
        return (_dispatcher);
    }

    /**
     * @brief Put back the dispatcher that was there, leaving the test's own in place for the handlers that cached it.
     */
    static void Finish()
    {
        WiFiRequestHandler::_messageDispatcher = _savedDispatcher;
    }

    /**
     * @brief Open the socket that is polled.
     */
    static int OpenPolledSocket()
    {
        const int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
        TEST_ASSERT_GREATER_OR_EQUAL_INT(0, descriptor);
        return (descriptor);
    }

    /**
     * @brief Make a poll request, as PollSetup does.
     */
    static WiFiRequestHandler::ActivePoll *Create(const int descriptor, const int32_t events, const uint32_t requestId, const int32_t timeoutMs = -1)
    {
        return (WiFiRequestHandler::CreateActivePoll(descriptor, events, requestId, timeoutMs));
    }

    /**
     * @brief Run PollSocket for a request in a task of its own.
     */
    static void StartWorker(WiFiRequestHandler::ActivePoll *activePoll, WorkerContext *context)
    {
        context->ActivePollPointer = activePoll;
        context->Done = xSemaphoreCreateBinary();
        TEST_ASSERT_NOT_NULL(context->Done);
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(WorkerTask, "PollWorker", 4096, context, 5, nullptr));
    }

    /**
     * @brief Wait for the worker task to finish.
     */
    static bool WaitForWorker(WorkerContext *context)
    {
        const bool finished = (xSemaphoreTake(context->Done, WORKER_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE);
        if (finished)
        {
            vSemaphoreDelete(context->Done);
            context->Done = nullptr;
        }
        return (finished);
    }

    /**
     * @brief Tear a request down, as the STM32's teardown does.
     */
    static int Teardown(const uint32_t requestId)
    {
        Esp32Messaging::PollRequest request = {};
        request.SetupMessageId = requestId;
        return (WiFiRequestHandler::PollTeardown(&request));
    }

    /**
     * @brief Number of requests that are active.
     */
    static size_t ActiveRequests()
    {
        Lock();
        const size_t count = WiFiRequestHandler::_pollRequests.size();
        Unlock();
        return (count);
    }

    /**
     * @brief Claim a request the way a teardown does, without waiting for the worker.
     *
     * @param activePoll Request to claim.
     * @param abandon true to say the teardown has given up waiting.
     */
    static void Claim(WiFiRequestHandler::ActivePoll *activePoll, const bool abandon)
    {
        Lock();
        activePoll->Cancelled = true;
        activePoll->Abandoned = abandon;
        WiFiRequestHandler::_pollRequests.erase(activePoll->RequestId);
        Unlock();
    }

    /**
     * @brief Whether the worker has said it has finished with a claimed request.
     */
    static bool WorkerDone(WiFiRequestHandler::ActivePoll *activePoll)
    {
        Lock();
        const bool done = activePoll->WorkerDone;
        Unlock();
        return (done);
    }

    /**
     * @brief The wake socket of a request.
     */
    static int WakeSocket(WiFiRequestHandler::ActivePoll *activePoll)
    {
        return (activePoll->Descriptors[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd);
    }

    /**
     * @brief The semaphore the worker gives for a claimed request.
     */
    static SemaphoreHandle_t Finished(WiFiRequestHandler::ActivePoll *activePoll)
    {
        return (activePoll->Finished);
    }

    /**
     * @brief Close the socket of a request and free it, as whoever comes last does.
     */
    static void Release(WiFiRequestHandler::ActivePoll *&activePoll)
    {
        WiFiRequestHandler::ReleaseActivePoll(activePoll);
    }

    /**
     * @brief Change how long a teardown waits for the poll to stop.
     *
     * @return The time it was.
     */
    static uint32_t SetCancelTimeout(const uint32_t milliseconds)
    {
        const uint32_t previous = WiFiRequestHandler::_pollCancelTimeout;
        WiFiRequestHandler::_pollCancelTimeout = milliseconds;
        return (previous);
    }

    /**
     * @brief Whether a descriptor is open.
     */
    static bool IsOpen(const int descriptor)
    {
        return (fcntl(descriptor, F_GETFL) >= 0);
    }

    /**
     * @brief Number of messages the handlers have sent the STM32 since the last call.
     */
    static int MessagesSent()
    {
        int count = 0;
        while (_dispatcher->OutboundMessageQueueLength() > 0)
        {
            Message *message = _dispatcher->GetOutboundMessage();
            Message::DeleteMessage(message);
            count++;
        }
        return (count);
    }

    /**
     * @brief Take the next message the handlers sent the STM32.
     */
    static Message *NextMessage()
    {
        return ((_dispatcher->OutboundMessageQueueLength() > 0) ? _dispatcher->GetOutboundMessage() : nullptr);
    }

private:
    /**
     * @brief The task that runs a worker.
     *
     * @param parameter The WorkerContext.
     */
    static void WorkerTask(void *parameter)
    {
        WorkerContext *context = static_cast<WorkerContext *>(parameter);
        SemaphoreHandle_t done = context->Done;
        WiFiRequestHandler::PollSocket(context->ActivePollPointer);
        xSemaphoreGive(done);
        vTaskDelete(nullptr);
    }

    /**
     * @brief The dispatcher the handlers had before the test.
     */
    static IMessageDispatcher *_savedDispatcher;

    /**
     * @brief The dispatcher the test gave them.
     */
    static MockMessageDispatcher *_dispatcher;
};

IMessageDispatcher *TestPollTeardown::_savedDispatcher = nullptr;
MockMessageDispatcher *TestPollTeardown::_dispatcher = nullptr;

TEST_CASE("Tearing down a blocked poll wakes it, waits for it, and sends no event", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x00000395;
    TestPollTeardown::MessagesSent();

    //
    //  Nothing is sent to an unbound UDP socket, so this poll blocks until it is woken.
    //
    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId);
    TEST_ASSERT_NOT_NULL(activePoll);
    const int wakeSocket = TestPollTeardown::WakeSocket(activePoll);
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);
    vTaskDelay(WORKER_SETTLING_TIME / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TestPollTeardown::Teardown(requestId), "Tearing down a blocked poll failed: the poll was not woken.");
    TEST_ASSERT_EQUAL_UINT32(0, TestPollTeardown::ActiveRequests());
    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::WaitForWorker(&context), "The worker did not finish.");

    //
    //  The wake socket is closed, once, by the teardown.  The request had the worker's writes to it
    //  (revents) finished before it was freed, so the heap is intact.
    //
    TEST_ASSERT_FALSE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The wake socket was left open.");
    TEST_ASSERT_TRUE_MESSAGE(heap_caps_check_integrity_all(true), "The heap was corrupted: the request was freed while the worker was using it.");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TestPollTeardown::MessagesSent(), "A poll that was torn down sent the STM32 a result.");

    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("A poll that completes sends its result and removes the request", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x00000396;
    TestPollTeardown::MessagesSent();

    //
    //  A UDP socket is always writable, so polling for that ends at once.
    //
    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLOUT, requestId);
    TEST_ASSERT_NOT_NULL(activePoll);
    const int wakeSocket = TestPollTeardown::WakeSocket(activePoll);
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);
    TEST_ASSERT_TRUE(TestPollTeardown::WaitForWorker(&context));

    Message *event = TestPollTeardown::NextMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "A poll that completed did not send the STM32 its result.");
    TEST_ASSERT_EQUAL_UINT8(MessageTypes::Event, event->MessageType);
    TEST_ASSERT_EQUAL_UINT32(WiFiFunction::InterruptPollResponse, event->Function);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::CompletedOk, event->StatusCode);
    TEST_ASSERT_NOT_NULL(event->Payload);
    Message::DeleteMessage(event);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, TestPollTeardown::ActiveRequests(), "A completed poll was left in the list.");
    TEST_ASSERT_FALSE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The worker left the wake socket open.");

    //
    //  The request is gone, so the teardown has nothing to stop, and succeeds without waiting.
    //
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::Teardown(requestId));
    TEST_ASSERT_TRUE(TestPollTeardown::IsOpen(descriptor));

    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("A poll with a timeout ends when it runs out, and sends its result with no events", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x0000039a;
    TestPollTeardown::MessagesSent();

    //
    //  Nothing is ever sent to the socket, so only the timeout can end the poll.
    //
    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId, 300);
    TEST_ASSERT_NOT_NULL(activePoll);
    const int wakeSocket = TestPollTeardown::WakeSocket(activePoll);
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);
    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::WaitForWorker(&context), "A poll with a timeout did not end when it ran out.");

    Message *event = TestPollTeardown::NextMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "A poll that timed out did not send the STM32 its result.");
    TEST_ASSERT_EQUAL_UINT8(MessageTypes::Event, event->MessageType);
    TEST_ASSERT_EQUAL_UINT32(WiFiFunction::InterruptPollResponse, event->Function);
    Message::DeleteMessage(event);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, TestPollTeardown::ActiveRequests(), "A poll that timed out was left in the list.");
    TEST_ASSERT_FALSE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The worker left the wake socket open.");

    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("Polls set up and torn down repeatedly leave nothing behind", CLASS_OF_TESTS)
{
    static const int ROUNDS = 40;

    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t heapBefore = esp_get_free_heap_size();

    //
    //  There are 16 sockets in all, so a wake socket left open by each poll runs out well before the end.
    //
    for (int round = 0; round < ROUNDS; round++)
    {
        const uint32_t requestId = 0x00039500 + round;
        TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId);
        TEST_ASSERT_NOT_NULL_MESSAGE(activePoll, "A poll could not be made: the sockets ran out.");
        WorkerContext context = {};
        TestPollTeardown::StartWorker(activePoll, &context);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, TestPollTeardown::Teardown(requestId), "A poll could not be torn down.");
        TEST_ASSERT_TRUE(TestPollTeardown::WaitForWorker(&context));
    }

    TEST_ASSERT_EQUAL_UINT32(0, TestPollTeardown::ActiveRequests());
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::MessagesSent());
    //
    //  A deleted task's stack is freed by the idle task, a moment after the task has gone.
    //
    vTaskDelay(200 / portTICK_PERIOD_MS);
    const uint32_t heapAfter = esp_get_free_heap_size();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(heapBefore > 1024 ? heapBefore - 1024 : 0, heapAfter, "Polls left heap behind.");

    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("A worker that starts after its poll was torn down does not poll", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x00000397;
    TestPollTeardown::MessagesSent();

    //
    //  Claimed, as a teardown does, before the worker has run.  If the worker polled it would block for ever,
    //  as nothing is going to wake it.
    //
    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId);
    TEST_ASSERT_NOT_NULL(activePoll);
    TestPollTeardown::Claim(activePoll, false);
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);

    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::WaitForWorker(&context), "The worker polled a request that had been torn down.");
    TEST_ASSERT_TRUE(TestPollTeardown::WorkerDone(activePoll));
    TEST_ASSERT_TRUE_MESSAGE(xSemaphoreTake(TestPollTeardown::Finished(activePoll), 0) == pdTRUE, "The worker did not say it had finished.");
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::MessagesSent());

    TestPollTeardown::Release(activePoll);
    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("A worker frees a request whose teardown stopped waiting", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x00000398;
    TestPollTeardown::MessagesSent();

    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId);
    TEST_ASSERT_NOT_NULL(activePoll);
    const int wakeSocket = TestPollTeardown::WakeSocket(activePoll);
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);
    vTaskDelay(WORKER_SETTLING_TIME / portTICK_PERIOD_MS);

    //
    //  The teardown claimed the request and gave up waiting, so it left the request for the worker to free.
    //  The worker is still in poll() when the claim is made.
    //
    TestPollTeardown::Claim(activePoll, true);
    TEST_ASSERT_EQUAL_INT(1, send(wakeSocket, "E", 1, 0));
    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::WaitForWorker(&context), "The worker did not come out of poll().");

    TEST_ASSERT_FALSE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The worker did not close the wake socket of an abandoned request.");
    TEST_ASSERT_TRUE_MESSAGE(heap_caps_check_integrity_all(true), "The heap was corrupted.");
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::MessagesSent());

    close(descriptor);
    TestPollTeardown::Finish();
}

TEST_CASE("Tearing down a poll that is not there succeeds without waiting", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const TickType_t start = xTaskGetTickCount();
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::Teardown(0x00000399));
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(1000 / portTICK_PERIOD_MS, xTaskGetTickCount() - start, "A teardown of nothing waited.");
    TestPollTeardown::Finish();
}

TEST_CASE("A teardown that cannot wake the poll leaves the request to the worker", CLASS_OF_TESTS)
{
    TestPollTeardown::Start();
    const int descriptor = TestPollTeardown::OpenPolledSocket();
    const uint32_t requestId = 0x0000039a;
    TestPollTeardown::MessagesSent();

    TestPollTeardown::Poll *activePoll = TestPollTeardown::Create(descriptor, POLLIN, requestId);
    TEST_ASSERT_NOT_NULL(activePoll);
    const int wakeSocket = TestPollTeardown::WakeSocket(activePoll);
    struct sockaddr_in wakeAddress = {};
    socklen_t length = sizeof(wakeAddress);
    TEST_ASSERT_EQUAL_INT(0, getsockname(wakeSocket, reinterpret_cast<struct sockaddr *>(&wakeAddress), &length));
    WorkerContext context = {};
    TestPollTeardown::StartWorker(activePoll, &context);
    vTaskDelay(WORKER_SETTLING_TIME / portTICK_PERIOD_MS);

    //
    //  Disconnect the wake socket, so that the byte the teardown sends it has nowhere to go and the worker stays
    //  in poll().  The teardown must not free a request that is still being polled.
    //
    struct sockaddr disconnect = {};
    disconnect.sa_family = AF_UNSPEC;
    TEST_ASSERT_EQUAL_INT(0, connect(wakeSocket, &disconnect, sizeof(disconnect)));
    const uint32_t previousTimeout = TestPollTeardown::SetCancelTimeout(300);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, TestPollTeardown::Teardown(requestId), "A teardown that did not stop the poll reported success.");
    TestPollTeardown::SetCancelTimeout(previousTimeout);
    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The wake socket was closed under a poll that was still using it.");
    TEST_ASSERT_TRUE_MESSAGE(heap_caps_check_integrity_all(true), "The heap was corrupted.");
    TEST_ASSERT_FALSE_MESSAGE(xSemaphoreTake(context.Done, 0) == pdTRUE, "The worker finished although nothing woke it.");

    //
    //  Wake it from outside.  It was left the request, so it closes the wake socket and frees the request.
    //
    TEST_ASSERT_EQUAL_INT(0, connect(wakeSocket, reinterpret_cast<struct sockaddr *>(&wakeAddress), sizeof(wakeAddress)));
    TEST_ASSERT_EQUAL_INT(1, send(wakeSocket, "E", 1, 0));
    TEST_ASSERT_TRUE_MESSAGE(TestPollTeardown::WaitForWorker(&context), "The worker did not come out of poll().");
    TEST_ASSERT_FALSE_MESSAGE(TestPollTeardown::IsOpen(wakeSocket), "The worker did not close the wake socket of an abandoned request.");
    TEST_ASSERT_TRUE_MESSAGE(heap_caps_check_integrity_all(true), "The heap was corrupted.");
    TEST_ASSERT_EQUAL_INT(0, TestPollTeardown::MessagesSent());

    close(descriptor);
    TestPollTeardown::Finish();
}
