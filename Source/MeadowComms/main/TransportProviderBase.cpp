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
 *      Get the number of messages in the outbound message queue.
 *
 *      @returns
 *          Number of messages in the outbound message queue.  This
 *          method will return -1 if the number of messages cannot be
 *          obtained.
 */
int TransportProviderBase::OutboundMessageQueueLength()
{
    int result = -1;
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
        xSemaphoreGive(_outboundMessageQueueMutex);
        ESP_LOGI(__func__, "Messages waiting: %d", messageCount);
        result = (int) messageCount;
    }
    return(result);
}

/*
 *  Get the next message from the message queue (if there is one available).
 *
 *  @return
 *      Pointer to the message that has been retrieved or nullptr if there
 *      are no messages in the queue or there is a problem getting the next
 *      message.
 */
Message *TransportProviderBase::GetOutboundMessage()
{
    Message *result = nullptr;
    UBaseType_t messageCount = OutboundMessageQueueLength();
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        if (messageCount > 0)
        {
            if (xQueueReceive(_outboundMessageQueueHandle, (void *) &result, portMAX_DELAY) != pdTRUE)
            {
                result = nullptr;
            }
        }
        xSemaphoreGive(_outboundMessageQueueMutex);
        ESP_LOGI(__func__, "Messages waiting: %d", messageCount);
    }
    return(result);
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
            else
            {
                ESP_LOGI(__func__, "Message queued.");
                gpio_set_level(Gpio::ESP32_READY_PIN, 1);
                vTaskDelay(50 / portTICK_PERIOD_MS);
                gpio_set_level(Gpio::ESP32_READY_PIN, 0);
            }
            UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
            xSemaphoreGive(_outboundMessageQueueMutex);
            ESP_LOGI(__func__, "Messages waiting: %d", messageCount);
        }
    }
    return(result);
}

