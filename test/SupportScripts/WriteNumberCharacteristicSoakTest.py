#!/usr/bin/env python

'''
The purpose of this script is to repeatedly write a number to a characteristic and read it back to verify the value.

It is the client for the Unity test "Soak test writing numeric characteristic, number of iterations controlled by
client (MeadowF7, Espressif)".  The firmware echoes each value it receives back into the characteristic, and this
script reads it to confirm the round trip.  Writing 0 tells the firmware the test is over.  The characteristic UUIDs
also match the Meadow managed code in Bluetooth_Basics.

A failed iteration disconnects without writing 0, so the firmware test fails as well rather than recording a pass.

Exit status is 0 when every iteration passed, 1 when an iteration failed and 2 when the device or characteristic
could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_DEVICE_NAME = 'MeadowF7'

ON_OFF_NAME = 'OnOff'
ON_OFF_UUID = '016e99d6-8a61-11eb-8dcd-0242ac1300ff'

BOOLEAN_FIELD_NAME = 'BooleanField'
BOOLEAN_FIELD_UUID = '017e99d6-8a61-11eb-8dcd-0242ac1300aa'

NUMBER_FIELD_NAME = 'NumberField'
NUMBER_FIELD_UUID = '018e99d6-8a61-11eb-8dcd-0242ac1300bb'

TEXT_FIELD_NAME = 'TextField'
TEXT_FIELD_UUID = '019e99d6-8a61-11eb-8dcd-0242ac1300cc'

DEFAULT_NUMBER_OF_ITERATIONS = 10000

DEFAULT_SCAN_TIMEOUT = 30.0

MAXIMUM_RETRIES = 10

RETRY_DELAY = 0.5

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)

async def write_and_verify(client, value):
    '''
    Write a value to the number characteristic and wait for the firmware to echo it back.

    :param client: The connected client.
    :param value: The value to write.
    :return: True when the value read back matches the value written.
    '''
    await client.write_gatt_char(NUMBER_FIELD_UUID, value.to_bytes(4, byteorder = 'little'))
    value_read = None
    for _ in range(MAXIMUM_RETRIES):
        response = await client.read_gatt_char(NUMBER_FIELD_UUID)
        value_read = int.from_bytes(response, byteorder = 'little')
        if value_read == value:
            return True
        await asyncio.sleep(RETRY_DELAY)
    logger.error('    [Read] Value mismatch after %d reads (iteration %d, read %s)', MAXIMUM_RETRIES, value, value_read)
    return False

async def main(args: argparse.Namespace):
    logger.info('Starting scan...')

    if args.address:
        device = await BleakScanner.find_device_by_address(args.address, timeout = args.scan_timeout, cb = dict(use_bdaddr = args.macos_use_bdaddr))
        if device is None:
            logger.error('Could not find device with address %s', args.address)
            return EXIT_NOT_FOUND
    else:
        device_name = args.name if args.name else DEFAULT_DEVICE_NAME
        device = await BleakScanner.find_device_by_name(device_name, timeout = args.scan_timeout, cb = dict(use_bdaddr = args.macos_use_bdaddr))
        if device is None:
            logger.error('Could not find device with name %s', device_name)
            return EXIT_NOT_FOUND

    logger.info('Connecting to device...')
    async with BleakClient(device, services = args.services) as client:
        logger.info('Connected')
        if client.services.get_characteristic(NUMBER_FIELD_UUID) is None:
            logger.error('Could not find the %s characteristic (%s)', NUMBER_FIELD_NAME, NUMBER_FIELD_UUID)
            return EXIT_NOT_FOUND

        logger.info('Starting write / read soak test, %s iterations.', f'{args.iterations:,}')
        for counter in range(1, args.iterations + 1):
            if (counter % 100 == 0) or (counter < 10):
                logger.info(f'    Iteration: {counter:,}')
            try:
                verified = await write_and_verify(client, counter)
            except Exception as e:
                logger.error(f'    Iteration: {counter:,}')
                logger.error('    [Write] Error: %s', e)
                verified = False
            if not verified:
                logger.info('Disconnecting without ending the test on the device.')
                return EXIT_FAILED
        #
        #   Use the value 0 to indicate that the test has completed.
        #
        await client.write_gatt_char(NUMBER_FIELD_UUID, (0).to_bytes(4, byteorder = 'little'))

        logger.info('Disconnecting...')
    logger.info('Disconnected')
    logger.info('Completed write / read soak test.')
    return EXIT_PASSED

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    device_group = parser.add_mutually_exclusive_group(required = False)
    device_group.add_argument('--name', metavar = '<name>', help = 'the name of the bluetooth device to connect to')
    device_group.add_argument('--address', metavar = '<address>', help = 'the address of the bluetooth device to connect to')

    parser.add_argument('--iterations', type = int, default = DEFAULT_NUMBER_OF_ITERATIONS, metavar = '<count>', help = f'number of values to write (default {DEFAULT_NUMBER_OF_ITERATIONS:,})')
    parser.add_argument('--scan-timeout', type = float, default = DEFAULT_SCAN_TIMEOUT, metavar = '<seconds>', help = f'seconds to look for the device (default {DEFAULT_SCAN_TIMEOUT:.0f})')
    parser.add_argument('--macos-use-bdaddr', action = 'store_true', help = 'when true use Bluetooth address instead of UUID on macOS')
    parser.add_argument('--services', nargs = '+', metavar = '<uuid>', help = 'if provided, only enumerate matching service(s)')
    parser.add_argument('-d', '--debug', action = 'store_true', help = 'sets the log level to debug')

    args = parser.parse_args()
    if args.iterations < 1:
        parser.error('--iterations must be at least 1')

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(level = log_level, format = '%(asctime)-15s %(name)-8s %(levelname)s: %(message)s')

    sys.exit(asyncio.run(main(args)))
