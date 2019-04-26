/*
 *      Esp32Messaging.cpp
 *
 *      Generic messaging constants, structures and definitions used
 *      in the messaging system.
 */
#include <string.h>

#include "Esp32Messaging.hpp"

/*
 *      Make a copy of a message.
 *
 *      @param originalMessage
 *          Original message to copy.
 *
 *      @param copyPayload
 *          Indicate if the payload should be copied as well or if we
 *          should just copy the header information.
 *
 *      @return
 *          New copy of the message.
 */
Message *CopyMessage(Message *originalMessage, bool copyPayload)
{
    Message *newMessage = new Message();
    newMessage->MessageType = originalMessage->MessageType;
    newMessage->Interface = originalMessage->Interface;
    newMessage->Function = originalMessage->Function;
    newMessage->StatusCode = originalMessage->StatusCode;
    newMessage->MessageID = originalMessage->MessageID;
    if (copyPayload)
    {
        newMessage->PayloadLength = originalMessage->PayloadLength;
        newMessage->Payload = (uint8_t *) pvPortMalloc(newMessage->PayloadLength);
        memcpy((void *) newMessage->Payload, (void *) originalMessage->Payload, (size_t) newMessage->Payload);
    }
    else
    {
        newMessage->PayloadLength = 0;
        newMessage->Payload = nullptr;
    }
    return(newMessage);
}

