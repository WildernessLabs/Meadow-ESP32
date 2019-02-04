#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "ESPFunctions.h"

#define HEXADECIMAL_HEADER_TEXT     "       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"

/*
 *      Synchronisation frame as defined in the ESP Serial Protocol documentation.
 */
uint8_t syncFrame[] =
{ 
    0x07, 0x07, 0x12, 0x20, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
    0x55, 0x55, 0x55, 0x55, 
};

/*
 *      Convert a buffer of bytes into a printable string.
 * 
 *      @param buffer
 *          Buffer of data to be converted to a printable string.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
char *BufferInHexadecimal(uint8_t *buffer, int length)
{
    char *result = (char *) malloc(1024);;
    char workingStorage[15];
    char text[17];
    int textIndex = 0;

    for (int index = 0; index < sizeof(text); index++)
    {
        text[index] = 0;
    }
    sprintf(result, HEXADECIMAL_HEADER_TEXT);
    for (int index = 0; index < length; index++)
    {
        if ((index % 16) == 0)
        {
            if (index != 0)
            {
                strcat(result, "  ");
                strcat(result, text);
                for (int index = 0; index < sizeof(text); index++)
                {
                    text[index] = 0;
                }
                textIndex = 0;
            }
            sprintf(workingStorage, "\n%04X  ", index);
            strcat(result, workingStorage);
        }
        sprintf(workingStorage, "%02X ", buffer[index]);
        strcat(result, workingStorage);
        if (isprint(buffer[index]))
        {
            text[textIndex] = buffer[index];
        }
        else
        {
            text[textIndex] = '.';
        }
        textIndex++;
    }
    if (textIndex > 0)
    {
        for (int index = textIndex; index < 16; index++)
        {
            strcat(result, "   ");
        }
        strcat(result, "  ");
        strcat(result, text);
    }
    strcat(result, "\n");
    return(result);
}

/*
 *      Calculate the number of bytes required for a decoded SLIP message.
 * 
 *      @param buffer
 *          Buffer of data to be converted to a printable string.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
int SlipDecodedBufferSize(uint8_t *buffer, int length)
{
    int result = length - 2;        // Remove the 0xC0 from the count.

    for (int index =0; index < length; index++)
    {
        if (buffer[index] == 0xDB)
        {
            result--;
        }
    }
    return(result);
}

/*
 *      Decode the buffer of SLIP encoded data into a the original byte sequence.
 * 
 *      @param buffer
 *          Buffer of data to be converted to a printable string.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
uint8_t SlipDecode(uint8_t *buffer, int length)
{
    int bufferSize = SlipDecodedBufferSize(buffer, length);
    uint8_t *result = (uint8_t *) malloc(bufferSize);
    uint8_t *source = buffer + 1;
    uint8_t *destination = result;

    while (source < (buffer + length - 1))
    {
        if (*source == 0xDB)
        {
            source++;
            if (*source == 0xDC)
            {
                *destination = 0xC0;
            }
            else
            {
                *destination = 0xDB;
            }
        }
        else
        {
            *destination = *source;
        }
        source++;
        destination++;
    }
    return(bufferSize);
}

/*
 *      Calculate the amount of storage required in order to store a SLIP encoded
 *      representation of the buffer.
 * 
 *      @param buffer
 *          Buffer of data to be converted to a printable string.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
int SlipEncodedBufferSize(uint8_t *buffer, int length)
{
    int result = length + 2;    // Packets must start and end with 0xC0.

    for (int index = 0; index < length; index++)
    {
        if ((buffer[index] == 0xC0) || (buffer[index] == 0xDB))
        {
            result++;
        }
    }
    return(result);
}

/*
 *      Encode a buffer of data according to the SLIP protocol.
 * 
 *      @param buffer
 *          Buffer of data encoded using the SLIP protocol.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
uint8_t *SlipEncode(uint8_t *buffer, int length)
{
    int bufferSize = SlipEncodedBufferSize(buffer, length);
    uint8_t *result = (uint8_t *) malloc(bufferSize);
    uint8_t *source = buffer;
    uint8_t *destination = result;

    *destination = 0xC0;
    while (source < (buffer + length))
    {
        switch (*source)
        {
            case 0xC0:
                *destination++ = 0xDB;
                *destination = 0xDC;
                break;
            case 0xDB:
                *destination++ = 0xDB;
                *destination = 0xDD;
                break;
            default:
                *destination = *source;
                break;
        }
        source++;
        destination++;
    }
    *destination = 0xC0;
    return(result);
}

/*
 *      Calculate the checksum for the buffer of data.
 * 
 *      @param buffer
 *          Buffer of data.
 * 
 *      @param length
 *          Amount of data in the buffer.
 */ 
uint8_t Checksum(uint8_t *buffer, int length)
{
    uint8_t checksum = 0xEF;

    for (int index = 0; index < length; index++)
    {
        checksum ^= buffer[index];
    }
    return(checksum);
}