/**
 * @file TransportProviderBase.cpp
 * @author Mark Stevens
 * @brief Base class for any hardware transport provider which facilitates communication between the ESP32 and STM32 chips.
 * @date 2019-02-22
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "sdkconfig.h"

#include "driver/gpio.h"

#include <string>

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
    if (_outboundMessageQueueHandle == NULL)
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
        result = (int) messageCount;
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
            if (xQueueReceive(_outboundMessageQueueHandle, (void *) &result, portMAX_DELAY) != pdTRUE)
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
    if (xQueuePeek(_outboundMessageQueueHandle, &(message), (TickType_t) 10))
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
 *  @param message
 *      Message to be sent to the STM32.
 *
 *  @return
 *      true if the message was queued, false otherwise.
 */
bool TransportProviderBase::QueueMessageForStm32(const Message * const message)
{
    bool result = true;

    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        if (xQueueSendToBack(_outboundMessageQueueHandle, &message, 0) != pdPASS)
        {
            ERROR_MESSAGE("Cannot queue message.");
            result = false;
        }
        else
        {
            Heap::CheckIntegrity();
            Logging::DumpMessage(COMPONENT_NAME, message);
            SendControlMessageToStm32(ControlMessageType::MessageWaiting);
        }
        xSemaphoreGive(_outboundMessageQueueMutex);
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
    char smallBuffer[20];

    switch (messageType)
    {
        case ControlMessageType::MessageWaiting:
            message = const_cast<char *>("+++MW\n");
            break;
        case ControlMessageType::Reset:
            {
                esp_reset_reason_t resetReason = esp_reset_reason();
                snprintf(smallBuffer, sizeof(smallBuffer), "\n+++RST,%" PRIi32 "\n", (int32_t) resetReason);
                message = smallBuffer;
            }
            break;
        case ControlMessageType::Ready:
            message = const_cast<char *>("+++RDY\n");
            break;
        case ControlMessageType::SpiInterfaceReady:
            message = const_cast<char *>("+++SIR\n");
            break;
        default:
            break;
    }
    if (message)
    {
        printf(message);
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