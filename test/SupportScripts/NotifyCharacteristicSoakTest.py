#!/usr/bin/env python

'''
Soak test the notification path: the device notifies a number, this script writes it back, and the device notifies
the next one.

It is the client for the Unity test "Soak test notifying characteristic, number of iterations controlled by client
(MeadowF7NotifyTest, Espressif)", and the UUIDs match the ProjLabV3SoakTestRunner managed application's
BluetoothNotifyTest.

1 - Subscribe to the notify field.
2 - Write SCRIPT_CONTROLLED_MARKER to the client number field, which says the test is driven by a script, and read it
    back once the device has echoed it.
3 - Write the number of iterations to the iteration count field and read that back too.
4 - The device notifies 1, 2, 3 and so on.  Each is written back to the client number field, and the device only
    notifies the next once it has seen the last.  A notification of 0 ends the test.

Exit status is 0 when every notification arrived in order, 1 when one was out of order or missing and 2 when the
device or a characteristic could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_DEVICE_NAME = 'MeadowF7NotifyTest'

ITERATION_COUNT_NAME = 'IterationCountField'
ITERATION_COUNT_UUID = '018e99d6-8a61-11eb-8dcd-0242ac1300aa'

CLIENT_NUMBER_FIELD_NAME = 'ClientNumberField'
CLIENT_NUMBER_FIELD_UUID = '018e99d6-8a61-11eb-8dcd-0242ac1300bb'

NOTIFY_FIELD_NAME = 'NotifyField'
NOTIFY_FIELD_UUID = '018e99d6-8a61-11eb-8dcd-0242ac1300cc'

SCRIPT_CONTROLLED_MARKER = 0x7fffffff

DEFAULT_NUMBER_OF_ITERATIONS = 10000

DEFAULT_SCAN_TIMEOUT = 30.0

DEFAULT_NOTIFY_TIMEOUT = 30.0

MAXIMUM_RETRIES = 10

RETRY_DELAY = 0.1

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)

async def set_characteristic_value(client, uuid, value):
    '''
    Write a value to a characteristic and wait for the device to echo it back.

    :param client: The connected client.
    :param uuid: The characteristic to write.
    :param value: The value to write.
    :return: True when the value read back matches the value written.
    '''
    await client.write_gatt_char(uuid, value.to_bytes(4, byteorder = 'little'))
    value_read = None
    for _ in range(MAXIMUM_RETRIES):
        response = await client.read_gatt_char(uuid)
        value_read = int.from_bytes(response, byteorder = 'little')
        if value_read == value:
            return True
        await asyncio.sleep(RETRY_DELAY)
    logger.error('[Read] %s, value mismatch after %d reads (wrote 0x%08x, read %s)', uuid, MAXIMUM_RETRIES, value, value_read)
    return False

def display_progress_counter(counter):
    '''
    Display the progress counter.

    :param counter: Notifications returned so far.
    '''
    if (counter < 10) or (counter % 100 == 0):
        logger.info(f'    Iteration: {counter:,}')

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

    #
    #   Notifications are queued and handled in order by the main loop rather than in the callback, so that a
    #   failure can end the test and the writes back are not interleaved with the setup writes.
    #
    notifications = asyncio.Queue()

    def notify_handler(characteristic, data: bytearray):
        notifications.put_nowait(int.from_bytes(data, byteorder = 'little'))

    logger.info('Connecting to device...')
    async with BleakClient(device, services = args.services) as client:
        logger.info('Connected')
        for uuid, name in ((ITERATION_COUNT_UUID, ITERATION_COUNT_NAME), (CLIENT_NUMBER_FIELD_UUID, CLIENT_NUMBER_FIELD_NAME), (NOTIFY_FIELD_UUID, NOTIFY_FIELD_NAME)):
            if client.services.get_characteristic(uuid) is None:
                logger.error('Could not find the %s characteristic (%s)', name, uuid)
                return EXIT_NOT_FOUND

        await client.start_notify(NOTIFY_FIELD_UUID, notify_handler)
        if not await set_characteristic_value(client, CLIENT_NUMBER_FIELD_UUID, SCRIPT_CONTROLLED_MARKER):
            return EXIT_FAILED
        if not await set_characteristic_value(client, ITERATION_COUNT_UUID, args.iterations):
            return EXIT_FAILED
        logger.info('Starting notify soak test, %s iterations.', f'{args.iterations:,}')

        expected = 1
        while True:
            try:
                value = await asyncio.wait_for(notifications.get(), timeout = args.notify_timeout)
            except asyncio.TimeoutError:
                logger.error('No notification within %.0f seconds, expecting %d.', args.notify_timeout, expected)
                return EXIT_FAILED
            if value == 0:
                break
            if value != expected:
                logger.error('Notified %d, expecting %d.', value, expected)
                return EXIT_FAILED
            await client.write_gatt_char(CLIENT_NUMBER_FIELD_UUID, value.to_bytes(4, byteorder = 'little'))
            display_progress_counter(value)
            expected += 1

        if expected - 1 != args.iterations:
            logger.error('The device ended the test after %d notifications, expecting %d.', expected - 1, args.iterations)
            return EXIT_FAILED
        logger.info('Disconnecting...')
    logger.info('Disconnected')
    logger.info('Completed notify soak test.')
    return EXIT_PASSED

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    device_group = parser.add_mutually_exclusive_group(required = False)
    device_group.add_argument('--name', metavar = '<name>', help = f'the name of the bluetooth device to connect to (default {DEFAULT_DEVICE_NAME})')
    device_group.add_argument('--address', metavar = '<address>', help = 'the address of the bluetooth device to connect to')

    parser.add_argument('--iterations', type = int, default = DEFAULT_NUMBER_OF_ITERATIONS, metavar = '<count>', help = f'number of notifications to ask for (default {DEFAULT_NUMBER_OF_ITERATIONS:,})')
    parser.add_argument('--scan-timeout', type = float, default = DEFAULT_SCAN_TIMEOUT, metavar = '<seconds>', help = f'seconds to look for the device (default {DEFAULT_SCAN_TIMEOUT:.0f})')
    parser.add_argument('--notify-timeout', type = float, default = DEFAULT_NOTIFY_TIMEOUT, metavar = '<seconds>', help = f'seconds to wait for each notification (default {DEFAULT_NOTIFY_TIMEOUT:.0f})')
    parser.add_argument('--macos-use-bdaddr', action = 'store_true', help = 'when true use Bluetooth address instead of UUID on macOS')
    parser.add_argument('--services', nargs = '+', metavar = '<uuid>', help = 'if provided, only enumerate matching service(s)')
    parser.add_argument('-d', '--debug', action = 'store_true', help = 'sets the log level to debug')

    args = parser.parse_args()
    if args.iterations < 1:
        parser.error('--iterations must be at least 1')

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(level = log_level, format = '%(asctime)-15s %(name)-8s %(levelname)s: %(message)s')

    sys.exit(asyncio.run(main(args)))
