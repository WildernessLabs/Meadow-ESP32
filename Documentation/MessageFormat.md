# Message Format

A message is made up of two parts:

* Message Header
* Payload (optional)

The Payload part of the message is optional as some tasks, say starting the network hardware do not require any additional information.  Other such as connecting to an access point require SSID and access point password and so these require a payload.

## Message Header

The message header contains enough information to allow the ESP32 to route the request to the correct interface and also ensure that the ESP32 can allocate enough space for the message to be processed.

The fields in the message header are:

* Interface ID
* Function
* Message ID
* Status code
* Packet offset
* Packet length
* Payload length
* CRC

### Interface ID

The interface ID indicates which interface should process the message.  The following interfaces are available:

* WiFi
* Bluetooth
* Mesh network
* System
* Wired ethernet (not used by the ESP32 code)

The interface is an 8-bit unsigned value.

### Function

The function bytes indicate the type of request being made to the receiver.

This is a 32-bit unsigned integer.

### Message ID

The Message ID field should be unique for each interface.  The presence of this field allows the sender to match any responses to the original request.  In this was the sender and receiver can be implemented as multi-threaded systems.

The message ID field is a 32-bit unsigned integer.  This allows for over 4 billion messages.  The _Message ID_ space is split into two, the top bit of the 32-bit field indicates the split.  This bit determines if the message originated on the STM32 or the ESP32.

### Status Code

The status code indicates the success or failure of the ESP32 system to deal with the request.  This is not the same as the result of a method call (for instance `sendto`).

### Packet Offset and Packet Length

Large messages (greater then ~4,000 bytes) are broken down into packets.  This is necessary as a bug in the SPI hardware on the ESP32 means that no single SPI transaction can be longer than about 4090 bytes.  Messages that require more than 4,000 bytes are broken down into smaller packets.  The _packet offset_ indicates where in the payload this packet of data should be placed.  The _packet length_ indicates the total number of bytes in this packet.

### Payload Length

The payload length is indicated by a 32-bit unsigned integer.  This represents the full length of the payload, not this packet.

The payload length may be zero if the message is simple.

### CRC

Each message sent has a 32-bit CRC value.  This is filled with 0 when the CRC is calculated and then populated with the 32-bit value before the message is sent.

The CRC shows that a message arrived as it was sent.  It does not show that the message is well formed: an STM32 that is working but wrong sends frames that arrive intact and carry bad lengths.  The receiver therefore checks the packet length, the payload length and every count and length inside a payload against the bytes it actually received, whatever the CRC says, and a message has to pass both those checks and the CRC.  The SPI bus has one peer, so integrity is all that is needed; the CRC is not an authentication.

## Message Payload

The message payload is a sequence of bytes that the interface will require in order to process the request.
