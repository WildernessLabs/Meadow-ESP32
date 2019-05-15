/*
 *      TransportProviderBase.cpp
 *
 *      Base class for any hardware transport provider which facilitates
 *      communication between the ESP32 and STM32 chips.
 */

#include "TransportProviderBase.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                   Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

QueueHandle_t TransportProviderBase::_outboundMessageQueueMutex = nullptr;
QueueHandle_t TransportProviderBase::_outboundMessageQueueHandle = nullptr;
TaskHandle_t TransportProviderBase::_transportProviderTaskHandle = nullptr;
/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Default constructor.
 */
TransportProviderBase::TransportProviderBase()
{
    _outboundMessageQueueMutex = xSemaphoreCreateMutex();
    if (!_outboundMessageQueueMutex)
    {
        ESP_LOGE(__func__, "Cannot create outbound message queue mutex.");
    }
}

/*
 *      Default destructor.
 */
TransportProviderBase::~TransportProviderBase()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                                 Methods
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Setup the hardware transport provider.
 */
void TransportProviderBase::Setup(MessageDispatcher *messageDispatcher)
{
    _messageDispatcher = messageDispatcher;
    _outboundMessageQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(Message *));
    if (_outboundMessageQueueHandle == NULL)
    {
        ESP_LOGE(__func__, "Cannot allocate hardware transport queue.");
    }
}

/*
 *      Add the message to the queue of messages waiting to be sent to
 *      the STM32.
 *
 *      @param message
 *          Message to be sent to the STM32.
 *
 *      @return
 *          true if the message was queued, false otherwise.
 */
bool TransportProviderBase::QueueMessageForStm32(Message *message)
{
    bool result = true;

    if ((message->Interface == Esp32Interfaces::System) && (message->Function == SystemFunction::Echo))
    {
        ESP_LOGI(__func__, "Echo received: %s", (char *) message->Payload);

    }
    else
    {
        if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
        {
            if (xQueueSendToBack(_outboundMessageQueueHandle, &message, 0) != pdPASS)
            {
                ESP_LOGE(__func__, "Cannot queue message.");
                result = false;
            }
            ESP_LOGI(__func__, "Message queued.");
            UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
            ESP_LOGI(__func__, "Messages waiting: %d", messageCount);
            xSemaphoreGive(_outboundMessageQueueMutex);
            if (xTaskNotify(_transportProviderTaskHandle, TransportProviderBase::OUTBOUND_MESSAGE_QUEUE_NOTIFICATION_BIT, eSetBits) != pdPASS)
            {
                ESP_LOGE(__func__, "Cannot send message queue notification.");
            }
        }
    }
    return(result);
}

