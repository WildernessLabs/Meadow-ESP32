#!/usr/bin/env python
'''
Client for the Unity test "A write without response and a long write reach the STM32 (MeadowWriteTest, Espressif)".

It writes 11 22 33 44 to the write characteristic without asking for a response, then writes a 512 byte value with a
response.  512 bytes does not fit in one packet, so the stack sends it as prepared writes followed by an execute.  The
firmware test checks that both values reach the STM32 intact (issue #299).

Exit status is 0 when both writes were sent, 1 when a write failed and 2 when the device or characteristic could not
be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_DEVICE_NAME = 'MeadowWriteTest'
WRITE_FIELD_UUID = '01ae99d6-8a61-11eb-8dcd-0242ac1300dd'

SHORT_VALUE = bytes([0x11, 0x22, 0x33, 0x44])
LONG_VALUE_LENGTH = 512

DEFAULT_SCAN_TIMEOUT = 30.0

#
#   Time given to the firmware to pass the write without response on before the long write follows.
#
SETTLE_SECONDS = 1.0

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)


def long_value():
    '''
    The long value the firmware test expects: (index * 3 + 1) & 0xff for each byte.
    :return: The value.
    '''
    return bytes(((index * 3) + 1) & 0xff for index in range(LONG_VALUE_LENGTH))


async def main(args: argparse.Namespace):
    logger.info('Starting scan...')
    device = await BleakScanner.find_device_by_name(args.name, timeout = args.scan_timeout)
    if device is None:
        logger.error('Could not find device with name %s', args.name)
        return EXIT_NOT_FOUND

    logger.info('Connecting to device...')
    async with BleakClient(device) as client:
        logger.info('Connected, MTU %d', client.mtu_size)
        if client.services.get_characteristic(WRITE_FIELD_UUID) is None:
            logger.error('Could not find the write characteristic (%s)', WRITE_FIELD_UUID)
            return EXIT_NOT_FOUND

        try:
            logger.info('Writing %d bytes without response.', len(SHORT_VALUE))
            await client.write_gatt_char(WRITE_FIELD_UUID, SHORT_VALUE, response = False)
            await asyncio.sleep(SETTLE_SECONDS)
            value = long_value()
            logger.info('Writing %d bytes with response.', len(value))
            await client.write_gatt_char(WRITE_FIELD_UUID, value, response = True)
            await asyncio.sleep(SETTLE_SECONDS)
        except Exception as e:
            logger.error('Write failed: %s', e)
            return EXIT_FAILED
        logger.info('Disconnecting...')
    logger.info('Disconnected')
    return EXIT_PASSED


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--name', default = DEFAULT_DEVICE_NAME, metavar = '<name>', help = f'the name of the bluetooth device to connect to (default {DEFAULT_DEVICE_NAME})')
    parser.add_argument('--scan-timeout', type = float, default = DEFAULT_SCAN_TIMEOUT, metavar = '<seconds>', help = f'seconds to look for the device (default {DEFAULT_SCAN_TIMEOUT:.0f})')
    parser.add_argument('-d', '--debug', action = 'store_true', help = 'sets the log level to debug')
    args = parser.parse_args()

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(level = log_level, format = '%(asctime)-15s %(name)-8s %(levelname)s: %(message)s')
    sys.exit(asyncio.run(main(args)))
