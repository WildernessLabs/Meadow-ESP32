#!/usr/bin/env python
'''
Client for the Unity test "Notifications are sent only to a client that has subscribed (MeadowSubscribe, Espressif)".

The firmware must send a notification only to a client that has enabled them in the characteristic's client
configuration, and a new connection must start unsubscribed (issue #572).  The client works through four steps.  After
each it writes the step number to the step characteristic, and the firmware sets the notifying characteristic to the
step number repeated in every nibble, 0x11111111 for step 1 and so on:

    1. Not subscribed.  No notification must arrive, but the value must be readable.
    2. Subscribed.  The notification must arrive.
    3. Unsubscribed again.  No notification must arrive, but the value must be readable.
    4. Subscribe and disconnect without unsubscribing, then connect again and do not subscribe.  No notification must
       arrive: the subscription must not outlive the connection.

The value is notified as it is set, so once a read returns the new value any notification for it has been sent, and the
client then waits a short while for one.

Exit status is 0 when every step behaved, 1 when one did not and 2 when the device or characteristic could not be found.
'''

import argparse
import asyncio
import logging
import sys

from bleak import BleakClient, BleakScanner

DEFAULT_DEVICE_NAME = 'MeadowSubscribe'
NOTIFY_UUID = '01be99d6-8a61-11eb-8dcd-0242ac1300ee'
STEP_UUID = '01ce99d6-8a61-11eb-8dcd-0242ac1300ff'

DEFAULT_SCAN_TIMEOUT = 30.0

#
#   How long to wait for the firmware to set a value, how long to listen for a notification that must not come once the
#   value has been seen, and how long to wait for one that must come.
#
VALUE_TIMEOUT = 15.0
QUIET_SECONDS = 2.0
NOTIFY_TIMEOUT = 10.0
POLL_SECONDS = 0.25

EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_NOT_FOUND = 2

logger = logging.getLogger(__name__)


def step_value(step):
    '''
    The value the firmware sets after a step: the step number repeated in every nibble.
    :param step: Step number, 1 to 4.
    :return: The value as the four little endian bytes it is sent in.
    '''
    return (step * 0x11111111).to_bytes(4, byteorder = 'little')


class Failed(Exception):
    '''A step did not behave as the firmware test requires.'''


async def finish_step(client, step):
    '''
    Tell the firmware a step is finished, so that it sets the value for it.
    :param client: The connected client.
    :param step: The step number.
    '''
    logger.info('Step %d: telling the firmware.', step)
    await client.write_gatt_char(STEP_UUID, step.to_bytes(4, byteorder = 'little'), response = True)


async def wait_for_value(client, step):
    '''
    Read the characteristic until it holds the value the firmware sets for the step.
    :param client: The connected client.
    :param step: The step number.
    '''
    expected = step_value(step)
    deadline = asyncio.get_running_loop().time() + VALUE_TIMEOUT
    value = None
    while asyncio.get_running_loop().time() < deadline:
        value = bytes(await client.read_gatt_char(NOTIFY_UUID))
        if value == expected:
            return
        await asyncio.sleep(POLL_SECONDS)
    raise Failed(f'Step {step}: the value never became {expected.hex()}; last read {None if value is None else value.hex()}.')


async def expect_no_notification(client, notifications, step):
    '''
    Check the value can be read and that no notification arrives for it.
    :param client: The connected client.
    :param notifications: Queue the notification handler fills.
    :param step: The step number.
    '''
    await wait_for_value(client, step)
    try:
        data = await asyncio.wait_for(notifications.get(), timeout = QUIET_SECONDS)
    except asyncio.TimeoutError:
        logger.info('Step %d: no notification, as expected.', step)
        return
    raise Failed(f'Step {step}: a notification arrived ({bytes(data).hex()}) though the client was not subscribed.')


async def expect_notification(notifications, step):
    '''
    Check the notification for the step arrives.
    :param notifications: Queue the notification handler fills.
    :param step: The step number.
    '''
    try:
        data = await asyncio.wait_for(notifications.get(), timeout = NOTIFY_TIMEOUT)
    except asyncio.TimeoutError:
        raise Failed(f'Step {step}: no notification within {NOTIFY_TIMEOUT:.0f} seconds though the client was subscribed.')
    if bytes(data) != step_value(step):
        raise Failed(f'Step {step}: expected {step_value(step).hex()}, received {bytes(data).hex()}.')
    logger.info('Step %d: notified %s, as expected.', step, bytes(data).hex())


async def main(args: argparse.Namespace):
    logger.info('Looking for %s...', args.name)
    device = await BleakScanner.find_device_by_name(args.name, timeout = args.scan_timeout)
    if device is None:
        logger.error('Could not find device with name %s', args.name)
        return EXIT_NOT_FOUND

    notifications = asyncio.Queue()

    def notify_handler(characteristic, data: bytearray):
        logger.info('Notification: %s', bytes(data).hex())
        notifications.put_nowait(bytes(data))

    try:
        async with BleakClient(device) as client:
            logger.info('Connected')
            if client.services.get_characteristic(NOTIFY_UUID) is None or client.services.get_characteristic(STEP_UUID) is None:
                logger.error('Could not find the characteristics.')
                return EXIT_NOT_FOUND

            await finish_step(client, 1)
            await expect_no_notification(client, notifications, 1)

            await client.start_notify(NOTIFY_UUID, notify_handler)
            await finish_step(client, 2)
            await expect_notification(notifications, 2)

            await client.stop_notify(NOTIFY_UUID)
            await finish_step(client, 3)
            await expect_no_notification(client, notifications, 3)

            #
            #   Subscribe again, then disconnect without unsubscribing.
            #
            await client.start_notify(NOTIFY_UUID, notify_handler)
            logger.info('Disconnecting while subscribed...')

        logger.info('Connecting again, without subscribing...')
        await asyncio.sleep(2.0)
        async with BleakClient(device) as client:
            logger.info('Connected')
            await finish_step(client, 4)
            await expect_no_notification(client, notifications, 4)
    except Failed as failure:
        logger.error('%s', failure)
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
