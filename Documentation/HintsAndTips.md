# Hints and Tips

## Main Program Loop

The main program loop does little more than instantiate instances of the `MessageDispatcher` and `SpiTransportProvider` classes.

```C++
SpiTransportProvider *spi = new SpiTransportProvider();
MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);
```

The code below these lines is normally commented out and remains in the source file as it provides a mechanism for sending a message through to the ESP32 message processing system without needing to involve the STM32.

```C++
Message message = {};
message.MessageType = MessageTypes::Header;
message.Interface = Esp32Interfaces::WiFi;
message.Function = WiFiFunction::GetAccessPoints;

messageDispatcher->QueueMessageForEsp32(&message);
```

The above is useful for debugging as it reduces the number of elements in the system being debugged.

It is often necessary to add a message payload to the message in order to pass data through to the function being requested.

## Messages and Responses

Messages payloads in the `Messages,json` file are normally defined in pairs having the postfix `Request` and `Response`.

The exception are messages that can be dealt with by one of two common responses:

* `IntegerResponse`
* `IntegerAndErrnoResponse`

The `IntegerResponse` message payload is used to response to messages such as the POSIX `socket` API.  This API returns a simple integer as it's response.

The `IntegerAndErrnoResponse` message payload is used to response to messages such as the POSIX `connect` API.  This method responses with an integer result to the method call.  It also sets `errno` to indicate any error condition that may have occurred.

## General Message Processing

Where possible the processing of the requests has followed the same formula for each request performing the following actions in sequence:

* Extraction of the request payload
* Free any payload resources to keep heap usage as low as possible
* Process the request
* Create response payload
* Use the original request header and add the response payload
* Free any heap resources that are no longer needed used in processing the request
* Queue the response


## `DispatchRequest` Method

`DispatchRequest` is the main entry point for requests from the STM32.  This is required as it is a pure virtual function on the `RequestHandlerBase` base class.