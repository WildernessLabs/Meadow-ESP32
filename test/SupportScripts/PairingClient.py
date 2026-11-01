#!/usr/bin/env python
'''
Client for the Unity test "A client pairs and bonds to use an encrypted characteristic (MeadowPair, Espressif)".

The characteristic needs an encrypted link to read or write it, so reading it makes the operating system pair with the
device (Just Works, with bonding, issue #398).  The operating system may ask for the pairing to be confirmed: click
Connect or Pair when it does.  A client that is already bonded encrypts with its stored keys and is not asked.

The client then:

    1. Checks the value it read is the one the firmware set ("PAIR").
    2. Subscribes to notifications and writes 1, the number of the connection.  The firmware checks the link was
       encrypted and the subscription recorded before the write arrived.
    3. Disconnects, connects again without subscribing, and writes 2.  The firmware checks the subscription was given
       back: a bonded client's subscription lasts from one connection to the next.

Exit status is 0 when every step behaved, 1 when one did not and 2 when the device or characteristic could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

DEFAULT_DEVICE_NAME = 'MeadowPair'
VALUE_UUID = '01ee99d6-8a61-11eb-8dcd-0242ac1300bb'

#
#   The value the firmware sets before the client connects, as the four little endian bytes it is read as.
#
EXPECTED_VALUE = (0x52494150).to_bytes(4, byteorder = 'little')

DEFAULT_SCAN_TIMEOUT = 30.0

#
#   Long enough for someone to confirm the pairing when the operating system asks.
#
PAIRING_TIMEOUT = 50.0

#
#   Time for the firmware to see each write before the client disconnects, and for the device to start advertising
#   again before the client looks for it.
#
SETTLE_SECONDS = 2.0

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)


class Failed(Exception):
    '''A step did not behave as the firmware test requires.'''


async def write_connection_number(client, connection):
    '''
    Write the number of the connection to the characteristic.
    :param client: The connected client.
    :param connection: The number of the connection, 1 or 2.
    '''
    logger.info('Connection %d: writing %d.', connection, connection)
    await client.write_gatt_char(VALUE_UUID, connection.to_bytes(4, byteorder = 'little'), response = True)
    await asyncio.sleep(SETTLE_SECONDS)


async def find(name, timeout):
    '''
    Look for the device.
    :param name: Name it advertises.
    :param timeout: Seconds to look.
    :return: The device, or None.
    '''
    logger.info('Looking for %s...', name)
    return await BleakScanner.find_device_by_name(name, timeout = timeout)


async def main(args: argparse.Namespace):
    device = await find(args.name, args.scan_timeout)
    if device is None:
        logger.error('Could not find device with name %s', args.name)
        return EXIT_NOT_FOUND

    def notify_handler(characteristic, data: bytearray):
        logger.info('Notification: %s', bytes(data).hex())

    try:
        async with BleakClient(device) as client:
            logger.info('Connected')
            if client.services.get_characteristic(VALUE_UUID) is None:
                logger.error('Could not find the characteristic.')
                return EXIT_NOT_FOUND

            logger.info('Connection 1: reading the encrypted characteristic; confirm the pairing if asked.')
            value = bytes(await asyncio.wait_for(client.read_gatt_char(VALUE_UUID), timeout = PAIRING_TIMEOUT))
            if value != EXPECTED_VALUE:
                raise Failed(f'Read {value.hex()}, expected {EXPECTED_VALUE.hex()}.')
            logger.info('Connection 1: read %s.', value.hex())

            await client.start_notify(VALUE_UUID, notify_handler)
            await write_connection_number(client, 1)
            logger.info('Disconnecting while subscribed...')

        await asyncio.sleep(SETTLE_SECONDS)
        device = await find(args.name, args.scan_timeout)
        if device is None:
            logger.error('Could not find device with name %s the second time', args.name)
            return EXIT_NOT_FOUND
        async with BleakClient(device) as client:
            logger.info('Connected again, not subscribing.')
            await write_connection_number(client, 2)
    except (Failed, BleakError, asyncio.TimeoutError) as failure:
        logger.error('%s', failure if str(failure) else type(failure).__name__)
        return EXIT_FAILED

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
