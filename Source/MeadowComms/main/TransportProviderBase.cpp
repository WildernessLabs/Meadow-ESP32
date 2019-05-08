/*
 * TransportProviderBase.cpp
 *
 *  Created on: 15 Feb 2019
 *      Author: mark
 */

#include "TransportProviderBase.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

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
    _queueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(Message));
    if (_queueHandle == NULL)
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
        if (xQueueSendToBack(_queueHandle, &message, 0) != pdPASS)
        {
            ESP_LOGE(__func__, "Cannot queue message.");
            result = false;
        }
    }
    return(result);
}

