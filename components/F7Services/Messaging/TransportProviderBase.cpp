/**
 * @file TransportProviderBase.cpp
 * @author Mark Stevens
 * @brief Base class for any hardware transport provider which facilitates communication between the ESP32 and STM32 chips.
 * @date 2019-02-22
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "TransportProviderBase.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"
#include "Heap.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of this component / task.
 */
const char *TransportProviderBase::COMPONENT_NAME = "TransportProviderBase";

QueueHandle_t TransportProviderBase::_outboundMessageQueueMutex = nullptr;
QueueHandle_t TransportProviderBase::_outboundMessageQueueHandle = nullptr;
TaskHandle_t TransportProviderBase::_transportProviderTaskHandle = nullptr;

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor.
 */
TransportProviderBase::TransportProviderBase()
{
    _outboundMessageQueueMutex = xSemaphoreCreateMutex();
    if (!_outboundMessageQueueMutex)
    {
        ERROR_MESSAGE("Cannot create outbound message queue mutex.");
    }
}

/**
 *  @brief Default destructor.
 */
TransportProviderBase::~TransportProviderBase()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the hardware transport provider.
 */
void TransportProviderBase::Setup(MessageDispatcher *messageDispatcher)
{
    _messageDispatcher = messageDispatcher;
    _outboundMessageQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(Message *));
    if (_outboundMessageQueueHandle == nullptr)
    {
        ERROR_MESSAGE("Cannot allocate hardware transport queue.");
    }
}

/**
 *  @brief Get the number of messages in the outbound message queue.
 *
 *  @returns
 *      Number of messages in the outbound message queue.  This
 *      method will return -1 if the number of messages cannot be
 *      obtained.
 */
int TransportProviderBase::OutboundMessageQueueLength()
{
    int result = -1;
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
        xSemaphoreGive(_outboundMessageQueueMutex);
        result = static_cast<int>(messageCount);
    }
    return (result);
}

/**
 *  @brief Get the next message from the message queue (if there is one available).
 *
 *  @return
 *  Pointer to the message that has been retrieved or nullptr if there
 *  are no messages in the queue or there is a problem getting the next
 *  message.
 */
Message *TransportProviderBase::GetOutboundMessage()
{
    Message *result = nullptr;
    // UBaseType_t messageCount = OutboundMessageQueueLength();
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        if (uxQueueMessagesWaiting(_outboundMessageQueueHandle) > 0)
        {
            if (xQueueReceive(_outboundMessageQueueHandle, &result, portMAX_DELAY) != pdTRUE)
            {
                result = nullptr;
            }
        }
        xSemaphoreGive(_outboundMessageQueueMutex);
    }

    return (result);
}

/**
 *  @brief Dump the message at the head of the message queue (if there is one)
 *  to as LOGI informaiton.
 *
 *  This method is intended for debugging.
 *
 *  @return
 *      None.
 */
void TransportProviderBase::DumpMessageAtHeadOfQueue()
{
    Message *message;
    if (xQueuePeek(_outboundMessageQueueHandle, &(message), static_cast<TickType_t>(10)))
    {
        Logging::DumpMessage(COMPONENT_NAME, message);
    }
    else
    {
        TRACE_MESSAGE("Message queue is empty.");
    }
}

/**
 *  @brief Add the message to the queue of messages waiting to be sent to
 *  the STM32.
 *
 *  The queue only has room for MAX_QUEUE_LENGTH messages and it is drained by the
 *  transport provider task when the STM32 asks for the next message.  A full queue
 *  is therefore usually a transient condition and the attempt is repeated up to
 *  QUEUE_ATTEMPTS times, pausing for QUEUE_ATTEMPT_DELAY in between, to give the
 *  STM32 the opportunity to collect the messages that are already waiting.
 *
 *  The queue mutex is released around the pause as the transport provider task
 *  needs it in order to take a message off the queue.
 *
 *  The mutex is held only for the queue operation itself.  The MessageWaiting control
 *  message is a blocking UART write, so sending it with the mutex held would make every
 *  other task queuing a message, and the transport provider task taking one off, wait
 *  on the UART (issue #271).  Sending it afterwards is safe: each message's
 *  MessageWaiting is sent after that message is on the queue, and the STM32 answers each
 *  one with a single request for the next message, so a request never finds the queue
 *  empty because of the order the two happen in.
 *
 *  A message that still cannot be queued is dropped and the Busy control signal is
 *  sent to the STM32 to indicate that there are messages waiting to be collected.
 *  Reporting the failure as a message to the STM32 is not possible as that would
 *  need a slot on the queue that is full.
 *
 *  @note This method takes ownership of the message.  The message is deleted if it
 *        cannot be queued, so the caller must not use it afterwards.
 *
 *  @param message
 *      Message to be sent to the STM32.
 *
 *  @return
 *      true if the message was queued, false otherwise.
 */
bool TransportProviderBase::QueueMessageForStm32(Message *message)
{
    bool result = false;
    //
    //  The transport provider task is the only task that takes messages off the queue and
    //  it only does so when the STM32 asks for one.  Pausing in that task would send the
    //  only task that can make room to sleep, so it is given a single attempt.
    //
    const uint32_t attempts = (xTaskGetCurrentTaskHandle() == _transportProviderTaskHandle) ? 1 : QUEUE_ATTEMPTS;

    //
    //  Both of these compile to nothing outside a diagnostic build.  The message is dumped
    //  here, while it still belongs to this task: once it is on the queue and the mutex is
    //  released, the transport provider task may take it off and delete it at any moment.
    //
    Heap::CheckIntegrity();
    Logging::DumpMessage(COMPONENT_NAME, message);

    for (uint32_t attempt = 0; (attempt < attempts) && !result; attempt++)
    {
        if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY) == pdTRUE)
        {
            result = (xQueueSendToBack(_outboundMessageQueueHandle, &message, 0) == pdPASS);
            xSemaphoreGive(_outboundMessageQueueMutex);
        }
        //
        //  The attempt failed, so pause to give the STM32 the chance to collect a
        //  message before trying again.  There is no point pausing after the last
        //  attempt as the message is about to be dropped.
        //
        if (!result && ((attempt + 1) < attempts))
        {
            vTaskDelay(QUEUE_ATTEMPT_DELAY);
        }
    }

    if (result)
    {
        SendControlMessageToStm32(ControlMessageType::MessageWaiting);
    }
    else
    {
        ERROR_MESSAGE("Cannot queue message after %" PRIu32 " attempt(s), dropping it.", attempts);
        SendControlMessageToStm32(ControlMessageType::Busy);
        Message::DeleteMessage(message);
    }

    return (result);
}

/**
 *  @brief Send a control message to the STM32.
 *
 *  @param messageType
 *     Type of control message to send.
 */
void TransportProviderBase::SendControlMessageToStm32(const ControlMessageType messageType)
{
    char const *message = nullptr;
    //
    //  "\n+++RST,%" PRIi32 "\n" with the most negative int32_t reset reason, plus the NUL
    //  terminator: 1 (leading \n) + 7 ("+++RST,") + 11 ("-2147483648") + 1 (trailing \n) +
    //  1 (NUL) = 21 bytes.  Using 32 to give some headroom.
    //
    char smallBuffer[32];

    switch (messageType)
    {
        case ControlMessageType::MessageWaiting:
            message = "+++MW\n";
            break;
        case ControlMessageType::Reset:
            {
                esp_reset_reason_t resetReason = esp_reset_reason();
                snprintf(smallBuffer, sizeof(smallBuffer), "\n+++RST,%" PRIi32 "\n", static_cast<int32_t>(resetReason));
                message = smallBuffer;
            }
            break;
        case ControlMessageType::Ready:
            message = "+++RDY\n";
            break;
        case ControlMessageType::SpiInterfaceReady:
            message = "+++SIR\n";
            break;
        case ControlMessageType::Busy:
            message = "+++BSY\n";
            break;
        default:
            break;
    }
    if (message)
    {
        printf("%s", message);
    }
}

/**
 * @brief Get the current control signal message that should be sent to the STM32.
 *
 * @return TransportProviderBase::ControlMessageType
 *      The current control signal message.
 */
TransportProviderBase::ControlMessageType TransportProviderBase::GetControlSignalMessage()
{
    return (_controlSignalMessage);
}

/**
 * @brief Set the control signal message to be sent to the STM32.
 *
 * @param messageType
 *      The control signal message to be sent.
 */
void TransportProviderBase::SetControlSignalMessage(const ControlMessageType messageType)
{
    _controlSignalMessage = messageType;
}

/**
 * @brief Get the number of ticks delay in the attempt to acquire the semaphore.
 *
 * @return TickType_t
 *      Number of ticks delay in the attempt to acquire the semaphore.
 */
TickType_t TransportProviderBase::GetControlSignalDelay()
{
    return (_controlSignalDelay);
}

/**
 * @brief Set the number of ticks delay in the attempt to acquire the semaphore.
 *
 * @param delay
 *      Number of ticks between attempts to take the semaphore.
 */
void TransportProviderBase::SetControlSignalDelay(const TickType_t delay)
{
    _controlSignalDelay = delay;
}