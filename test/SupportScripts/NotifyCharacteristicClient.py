#!/usr/bin/env python

'''
Subscribe to a characteristic and check the value the device notifies.

It is the client for the Unity notify tests, "Notify characteristic (MeadowC, no company identifier)" and "Notify
characteristic, multiple descriptors (MeadowD, no company identifier)".  Once the client has connected the firmware
allows it a few seconds to subscribe and then writes a known value to the characteristic, which the stack sends on
as a notification.

The client disconnects once it has checked the value; the firmware test waits for that before stopping the service.

Exit status is 0 when the expected value was notified, 1 when a different value or no value arrived and 2 when the
device or characteristic could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_CHARACTERISTIC_UUID = '017e99d6-8a61-11eb-8dcd-0242ac130003'

DEFAULT_VALUE = 0xa0a0a0a0

DEFAULT_SCAN_TIMEOUT = 30.0

DEFAULT_NOTIFY_TIMEOUT = 30.0

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)

async def main(args: argparse.Namespace):
    logger.info('Looking for %s...', args.name)
    device = await BleakScanner.find_device_by_name(args.name, timeout = args.scan_timeout, cb = dict(use_bdaddr = args.macos_use_bdaddr))
    if device is None:
        logger.error('Could not find device with name %s', args.name)
        return EXIT_NOT_FOUND

    notified = asyncio.get_running_loop().create_future()

    def notify_handler(characteristic, data: bytearray):
        logger.info('Notification: %s', data.hex())
        if not notified.done():
            notified.set_result(bytes(data))

    logger.info('Connecting to device...')
    async with BleakClient(device) as client:
        logger.info('Connected')
        characteristic = client.services.get_characteristic(args.characteristic)
        if characteristic is None:
            logger.error('Could not find characteristic %s', args.characteristic)
            return EXIT_NOT_FOUND
        logger.info('Characteristic properties: %s', ', '.join(characteristic.properties))

        await client.start_notify(characteristic, notify_handler)
        logger.info('Subscribed, waiting up to %.0f seconds for a notification.', args.notify_timeout)
        try:
            data = await asyncio.wait_for(notified, timeout = args.notify_timeout)
        except asyncio.TimeoutError:
            logger.error('No notification within %.0f seconds.', args.notify_timeout)
            return EXIT_FAILED

        if data != args.value.to_bytes(4, byteorder = 'little'):
            logger.error('Expected 0x%08x, received %s', args.value, data.hex())
            return EXIT_FAILED
        logger.info('Received the expected value 0x%08x.', args.value)
        logger.info('Disconnecting...')
    logger.info('Disconnected')
    return EXIT_PASSED

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('--name', required = True, metavar = '<name>', help = 'the name of the bluetooth device to connect to')
    parser.add_argument('--characteristic', default = DEFAULT_CHARACTERISTIC_UUID, metavar = '<uuid>', help = f'characteristic to subscribe to (default {DEFAULT_CHARACTERISTIC_UUID})')
    parser.add_argument('--value', type = lambda text: int(text, 0), default = DEFAULT_VALUE, metavar = '<value>', help = f'value expected in the notification, little endian (default 0x{DEFAULT_VALUE:08x})')
    parser.add_argument('--scan-timeout', type = float, default = DEFAULT_SCAN_TIMEOUT, metavar = '<seconds>', help = f'seconds to look for the device (default {DEFAULT_SCAN_TIMEOUT:.0f})')
    parser.add_argument('--notify-timeout', type = float, default = DEFAULT_NOTIFY_TIMEOUT, metavar = '<seconds>', help = f'seconds to wait for the notification (default {DEFAULT_NOTIFY_TIMEOUT:.0f})')
    parser.add_argument('--macos-use-bdaddr', action = 'store_true', help = 'when true use Bluetooth address instead of UUID on macOS')
    parser.add_argument('-d', '--debug', action = 'store_true', help = 'sets the log level to debug')

    args = parser.parse_args()

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(level = log_level, format = '%(asctime)-15s %(name)-8s %(levelname)s: %(message)s')

    sys.exit(asyncio.run(main(args)))
