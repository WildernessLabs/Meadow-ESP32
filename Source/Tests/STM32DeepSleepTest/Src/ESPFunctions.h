#ifndef _ESPFUNCTIONS_H_

#define _ESPFUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef __cpluplus
    extern "C"
    {
#endif

/*
 *      Commands that can be issued.
 */
enum Commands
{
    FlashBefing = 0x02,
    FlashData = 0x03,
    FlashEnd = 0x04,
    MemBegin = 0x05,
    MemEnd = 0x06,
    MemData = 0x07,
    Synchronise = 0x08,
    WriteRegister = 0x09,
    ReadRegister = 0x0A,
    SpiSetParams = 0x0B,
    SpiAttach = 0x0D,
    ChangeBaudrate = 0x0F,
    FlashDeflBegin = 0x10,
    FlashDeflData = 0x11,
    FlashDeflEnd = 0x12,
    SpiFlashMd5 = 0x13,
    EraseFlash = 0xD0,
    EraseRegion = 0xD1,
    ReadFlash = 0xD2,
    RunUserCode = 0xD3
};

/*
 *      Direction of a message.
 */
enum Direction
{
    Request = 0x00,
    Response = 0x01
};

/*
 *      Status codes that can be returned by the ESP32.
 */
enum StatusCodes
{
  Ok = 0,
  BadDataLength = 0xC0,
  BadDataChecksum = 0xC1,
  BadBlocksize = 0xC2,
  InvalidCommand = 0xC3,
  FailedSpiOperation = 0xC4,
  FailedSpiUnlock = 0xC5,
  NotInFlashMode = 0xC6,
  InflateError = 0xC7,
  NotEnoughData = 0xC8,
  TooMuchData = 0xC9,
  NotImplemented= 0xFF
};

/*
 *      Various communication states possible when talking to the ESP32.
 */
enum CommunicationState
{
    Synchronising,
    GettingChipID,
    Unknown
};

/*
 *      Synchronisation frame.
 */
extern uint8_t syncFrame[];
#define SIZE_OF_SYNC_FRAME	        36

/*
 *      External functions.
 */
char *BufferInHexadecimal(uint8_t *, int);
int SlipDecodedBufferSize(uint8_t *, int);
uint8_t SlipDecode(uint8_t *, int);
int SlipEncodedBufferSize(uint8_t *, int);
uint8_t *SlipEncode(uint8_t *, int);
uint8_t Checksum(uint8_t *, int);

#ifdef __cpluplus
    }
#endif

#endif 