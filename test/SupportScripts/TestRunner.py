#!/usr/bin/env python

'''
Drive the interactive Unity test menu and report the results.

The test application built from the test directory ends in unity_run_menu(), which is
a command interpreter on the serial console.  This script talks to that interpreter so
the tests can be run without anyone typing at a terminal.  The same menu is presented
whether the firmware is running on hardware or under QEMU, so one driver serves both;
only the transport differs.

Selectors are passed through to the menu unchanged and may be:

    [FileSystem]        every test carrying that tag
    *                   every registered test
    "Delete file"       a single test, by name, with the quotes
    7                   a single test, by menu number

Exit status is 0 when every selector passed, 1 when a test failed, 2 when a selector
produced no result before the timeout, and 3 when the device rebooted part way through.
'''

import argparse
import os
import re
import socket
import subprocess
import sys
import time

#
#   Text the firmware prints at the points the driver has to synchronise with.
#
BOOT_PROMPT = 'Press ENTER to see the list of tests.'
MENU_PROMPT = "Enter next test, or 'enter' to see menu"
SUMMARY = re.compile(r'^(\d+) Tests (\d+) Failures (\d+) Ignored', re.M)

#
#   qemu.sh lives in the root of the repository, two directories above this script.
#
QEMU_SCRIPT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'qemu.sh'))

DEFAULT_SERIAL_PORT = 5555
DEFAULT_BAUD = 115200


class Transport:
    '''Common interface over a QEMU socket and a hardware serial port.'''

    def read(self):
        '''Return whatever bytes are available, or b'' if none arrive promptly.'''
        raise NotImplementedError

    def write(self, data):
        '''Send bytes to the device.'''
        raise NotImplementedError

    def close(self):
        '''Release the transport and anything started alongside it.'''
        raise NotImplementedError


class QemuTransport(Transport):
    '''
    Runs the image under QEMU and talks to its console over TCP.

    The emulator is started through qemu.sh rather than directly, so the machine
    arguments, the eFuse wiring and the pristine image handling are defined in exactly
    one place.  That script execs QEMU, so the process started here is the emulator
    itself and stopping it needs no further bookkeeping.
    '''

    def __init__(self, buildDirectory, port, fresh, qemuScript):
        self._buildDirectory = buildDirectory

        command = [
            qemuScript,
            f'--build-dir={buildDirectory}',
            '--serial-tcp',
            f'--serial-port={port}',
        ]
        if fresh:
            command.append('--fresh')

        self._process = subprocess.Popen(command,
                                         stdout=subprocess.DEVNULL,
                                         stderr=subprocess.DEVNULL)

        #
        #   QEMU needs a moment to open its listening socket.
        #
        self._socket = None
        for _ in range(30):
            try:
                self._socket = socket.create_connection(('localhost', port), timeout=2)
                break
            except OSError:
                time.sleep(0.5)
        if self._socket is None:
            self._process.kill()
            raise RuntimeError(f'QEMU did not open a console on port {port}')
        self._socket.settimeout(1.0)

    def read(self):
        try:
            return self._socket.recv(4096)
        except socket.timeout:
            return b''

    def write(self, data):
        self._socket.sendall(data)

    def close(self):
        self._socket.close()
        self._process.kill()
        self._process.wait()


class SerialTransport(Transport):
    '''Talks to a board over its console UART.'''

    def __init__(self, device, baud, fresh):
        import serial

        self._port = serial.Serial(device, baud, timeout=1)

        #
        #   Toggling RTS resets the board, which is the closest hardware equivalent of
        #   restoring a pristine flash image: it guarantees the run starts from boot
        #   rather than from whatever state the previous run left behind.
        #
        if fresh:
            self._port.setDTR(False)
            self._port.setRTS(True)
            time.sleep(0.15)
            self._port.setRTS(False)
            self._port.reset_input_buffer()

    def read(self):
        return self._port.read(4096)

    def write(self, data):
        self._port.write(data)
        self._port.flush()

    def close(self):
        self._port.close()


def ReadUntil(transport, predicate, timeout, transcript):
    '''
    Accumulate console output until predicate reports the text is complete.

    Returns the text read and whether the predicate was satisfied before the timeout.
    '''
    deadline = time.time() + timeout
    text = ''
    while time.time() < deadline:
        chunk = transport.read()
        if not chunk:
            continue
        decoded = chunk.decode('utf-8', errors='replace')
        text += decoded
        transcript.append(decoded)
        if predicate(text):
            return (text, True)
    return (text, False)


def RunSelector(transport, selector, timeout, transcript):
    '''
    Run one selector and return its result as a tuple of status and counts.

    The selector is only written once the menu prompt has been seen.  unity_gets()
    discards anything already in the receive buffer before it reads, so a selector sent
    ahead of the prompt is silently thrown away and the run appears to hang.
    '''
    mark = len(transcript)
    transport.write((selector + '\n').encode())

    def Settled(text):
        return MENU_PROMPT in text or BOOT_PROMPT in text

    ReadUntil(transport, Settled, timeout, transcript)
    segment = ''.join(transcript[mark:])

    if BOOT_PROMPT in segment:
        return ('rebooted', segment, None)

    match = SUMMARY.search(segment)
    if match is None:
        return ('stalled', segment, None)

    total, failures, ignored = (int(value) for value in match.groups())
    status = 'passed' if failures == 0 else 'failed'
    return (status, segment, (total, failures, ignored))


def main():
    parser = argparse.ArgumentParser(description='Run the Meadow ESP32 unit tests.')
    parser.add_argument('selectors', nargs='*', default=['*'],
                        help='Menu selectors to run (default: every test)')
    parser.add_argument('--build-dir', required=True,
                        help='Build directory holding the QEMU image or the test ELF')
    parser.add_argument('--hardware', action='store_true',
                        help='Run against a connected board instead of QEMU')
    parser.add_argument('--device', default=None,
                        help='Serial device of the board, required with --hardware')
    parser.add_argument('--baud', type=int, default=DEFAULT_BAUD,
                        help=f'Console baud rate (default {DEFAULT_BAUD})')
    parser.add_argument('--serial-port', type=int, default=DEFAULT_SERIAL_PORT,
                        help=f'TCP port for the QEMU console (default {DEFAULT_SERIAL_PORT})')
    parser.add_argument('--timeout', type=int, default=300,
                        help='Seconds to allow each selector (default 300)')
    parser.add_argument('--fresh', action='store_true',
                        help='Reset to a known state before running')
    parser.add_argument('--transcript', default=None,
                        help='Write the full console transcript to this file')
    parser.add_argument('--verbose', action='store_true',
                        help='Echo the console output as tests run')
    arguments = parser.parse_args()

    if arguments.hardware and arguments.device is None:
        parser.error('--hardware requires --device')

    transcript = []

    if arguments.hardware:
        transport = SerialTransport(arguments.device, arguments.baud, arguments.fresh)
        print(f'Running tests on {arguments.device}.')
    else:
        transport = QemuTransport(arguments.build_dir, arguments.serial_port,
                                  arguments.fresh, QEMU_SCRIPT)
        print('Running tests under QEMU.')

    status = 0
    try:
        _, booted = ReadUntil(transport, lambda text: BOOT_PROMPT in text, 60, transcript)
        if not booted:
            print('The test menu was never reached.')
            print(''.join(transcript)[-2000:])
            return 2

        for selector in arguments.selectors:
            print(f'\n=== {selector} ===')
            result, segment, counts = RunSelector(transport, selector,
                                                  arguments.timeout, transcript)
            if arguments.verbose or result in ('stalled', 'rebooted'):
                print(segment.strip())

            if result == 'rebooted':
                print(f'{selector}: the device rebooted part way through.')
                status = 3
                break
            if result == 'stalled':
                print(f'{selector}: no result within {arguments.timeout}s.')
                status = 2
                break

            total, failures, ignored = counts
            print(f'{selector}: {total} tests, {failures} failures, {ignored} ignored.')
            if failures and status == 0:
                status = 1
    finally:
        if arguments.transcript:
            with open(arguments.transcript, 'w') as handle:
                handle.write(''.join(transcript))
        transport.close()

    return status


if __name__ == '__main__':
    sys.exit(main())
