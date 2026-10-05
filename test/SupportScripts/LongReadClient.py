#!/usr/bin/env python
'''
Client for the Unity test "A value longer than the MTU is read in full (MeadowReadTest, Espressif)".

It reads the 517 byte value of the read characteristic and checks it is the one the firmware set,
(index * 7 + 3) & 0xff for each byte.  A read returns at most the MTU minus one byte, so the stack follows it
with Read Blob Requests at increasing offsets.  Before issue #528 each of those was answered with the start of
the value again.  517 bytes is more than a read returns at the firmware's 500 byte MTU, so the long read
happens whatever MTU is negotiated.

Exit status is 0 when the value read is the one expected, 1 when it is not or the read failed and 2 when the
device or characteristic could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_DEVICE_NAME = 'MeadowReadTest'
READ_FIELD_UUID = '01de99d6-8a61-11eb-8dcd-0242ac130100'

VALUE_LENGTH = 517

DEFAULT_SCAN_TIMEOUT = 30.0

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)


def expected_value():
    '''
    The value the firmware test sets: (index * 7 + 3) & 0xff for each byte.
    :return: The value.
    '''
    return bytes(((index * 7) + 3) & 0xff for index in range(VALUE_LENGTH))


async def main(args: argparse.Namespace):
    logger.info('Starting scan...')
    device = await BleakScanner.find_device_by_name(args.name, timeout = args.scan_timeout)
    if device is None:
        logger.error('Could not find device with name %s', args.name)
        return EXIT_NOT_FOUND

    logger.info('Connecting to device...')
    async with BleakClient(device) as client:
        logger.info('Connected, MTU %d', client.mtu_size)
        if client.services.get_characteristic(READ_FIELD_UUID) is None:
            logger.error('Could not find the read characteristic (%s)', READ_FIELD_UUID)
            return EXIT_NOT_FOUND

        try:
            value = bytes(await client.read_gatt_char(READ_FIELD_UUID))
        except Exception as e:
            logger.error('Read failed: %s', e)
            return EXIT_FAILED

        expected = expected_value()
        logger.info('Read %d bytes, expected %d.', len(value), len(expected))
        if value != expected:
            first = next((index for index in range(min(len(value), len(expected))) if value[index] != expected[index]), min(len(value), len(expected)))
            logger.error('The value read is not the one set: it first differs at byte %d.', first)
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
