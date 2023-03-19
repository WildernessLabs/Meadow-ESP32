# Interrupt Processing

The messaging system supports the generation of interrupt / event / callback messages using the `Interrupt` message type.

These messages can be generated in response to a message or due to an arbitrary event such as an error, network disconnect, bluetooth serial data reception etc.

## ESP32 Interrupt Generation

Creation of an interrupt follows the same mechanism of generating any other message for transmission to the STM32.

* Create the message header
* Attach any payload for the message
* Set the message type to `Interrupt`
* Add the message to the queue of messages for the STM32

## ESP32 Coprocessor on the STM32

An interrupt message sent to the STM32 will finally be dispatched to the `espcp_dispatch_interrupt` method in `espcp_interrupt_handlers.c` file.  This method will use the interface type and the function fields to determine the method that will eventually be called to process the message from the ESP32 (the interrupt handler).  The interrupt handler is responsible for processing the message data and releasing the storage used to hold the message.

Each interface on the ESP32 is allocated a table of interrupt handlers:

```C++
static espcp_interrupt_handlers_t _wifi_handlers[] = 
{
    { espcp_wi_fi_function_interrupt_poll_response, espcp_usrsock_poll_interrupt_handler },
    { 0xffffffff, NULL }
};
```
The interrupt dispatchers use these tables to locate the appropriate interrupt handler.

## C# Events

TBC