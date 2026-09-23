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

A test that deliberately restarts the device, such as the one that forces a core dump,
ends its selector early and leaves the device back at the boot prompt.  Naming that
selector with --expect-reboot tells the driver to carry on with the next selector rather
than treating the restart as a failure, which allows the tests that read back what the
restart produced to run in the same session and so against the same flash.

Note that this works between selectors and not within one.  The menu runs a whole tag as
a single batch on the device, and a restart part way through loses its place; offering
the tag again would start it from the beginning.  A test that restarts the device
therefore has to be named as a selector of its own, ahead of the selectors that depend
on it.

Some Bluetooth tests wait for a client to connect over the air.  When one of those asks
for its client, the matching script from this directory is started alongside it with
the interpreter named by --bluetooth-python, which needs the packages listed in
requirements.txt.  A client that fails fails the selector.

Exit status is 0 when every selector passed, 1 when a test failed, 2 when a selector
produced no result before the timeout, and 3 when the device rebooted unexpectedly.
'''

import argparse
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
from xml.dom import minidom

#
#   Text the firmware prints at the points the driver has to synchronise with.
#
BOOT_PROMPT = 'Press ENTER to see the list of tests.'
MENU_PROMPT = "Enter next test, or 'enter' to see menu"
SUMMARY = re.compile(r'^(\d+) Tests (\d+) Failures (\d+) Ignored', re.M)

#
#   Unity prints one of these for every test it runs, for example:
#       /path/TestFileSystem.cpp:206:Formatting file system:PASS
#
#   The trailing \r is matched explicitly because the console sends \r\n and the end of
#   line anchor would otherwise never be reached.
#
UNITY_RESULT = re.compile(
    r'^(/[^:\n]+):(\d+):(.+?):(PASS|FAIL|IGNORE)(?::(.*?))?\r?$', re.M)

#
#   qemu.sh lives in the root of the repository, two directories above this script.
#
QEMU_SCRIPT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'qemu.sh'))

#
#   How many times a selector is offered to the menu, and how long each attempt waits
#   for the device to echo it back, before the selector is treated as ignored.
#
SELECTOR_ATTEMPTS = 5
SELECTOR_ACKNOWLEDGE_SECONDS = 5

DEFAULT_SERIAL_PORT = 5555
DEFAULT_BAUD = 115200

#
#   The Bluetooth tests that need a client print CLIENT_PROMPT once their service is
#   advertising.  Unity names each test as it starts it, and that name selects the
#   client from BLUETOOTH_CLIENTS.
#
CLIENT_PROMPT = 'Please connect client within'
TEST_STARTED = re.compile(r'^Running (.+)\.\.\.$')
SUPPORT_SCRIPTS = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SOAK_ITERATIONS = 1000

#
#   How long a client is given to finish once its test has reported a result.
#
CLIENT_FINISH_SECONDS = 30

BLUETOOTH_CLIENTS = {
    'Soak test writing numeric characteristic, number of iterations controlled by client (MeadowF7, Espressif)':
        lambda arguments: ['WriteNumberCharacteristicSoakTest.py', '--iterations', str(arguments.soak_iterations)],
}


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

        self._process = subprocess.Popen(command, stdout=subprocess.DEVNULL)

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


class ClientLauncher:
    '''
    Starts the Bluetooth client a test asks for, while the test is waiting for it.

    The console is watched line by line.  When a test prompts for a client, the client
    registered against that test's name is started alongside it, using the interpreter
    given, which has to have bleak installed.
    '''

    def __init__(self, python, arguments):
        self._python = python
        self._arguments = arguments
        self._pending = ''
        self._currentTest = None
        self._clients = []

    def Observe(self, text):
        '''Process console output as it arrives.'''
        self._pending += text
        lines = self._pending.split('\n')
        self._pending = lines.pop()
        for line in lines:
            line = line.rstrip('\r')
            started = TEST_STARTED.match(line)
            if started:
                self._currentTest = started.group(1)
            elif CLIENT_PROMPT in line:
                self._Launch()

    def _Launch(self):
        '''Start the client for the current test, if one is registered.'''
        client = BLUETOOTH_CLIENTS.get(self._currentTest)
        if client is None:
            print(f'No Bluetooth client is registered for "{self._currentTest}"; it will time out.')
            return
        command = client(self._arguments)
        command = [self._python, os.path.join(SUPPORT_SCRIPTS, command[0])] + command[1:]
        print(f'Starting Bluetooth client {os.path.basename(command[1])}.')
        output = tempfile.TemporaryFile(mode='w+')
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, text=True)
        self._clients.append((self._currentTest, process, output))

    def Finish(self, verbose):
        '''
        Wait for the clients started for a selector and report on them.

        Returns True when every client exited cleanly.  A client that is still running
        after its test has reported is stopped: the test cannot use it any more.
        '''
        succeeded = True
        for test, process, output in self._clients:
            try:
                process.wait(timeout=CLIENT_FINISH_SECONDS)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                print(f'The Bluetooth client for "{test}" was still running and has been stopped.')
            output.seek(0)
            log = output.read()
            output.close()
            if process.returncode != 0:
                succeeded = False
                print(f'The Bluetooth client for "{test}" failed with exit status {process.returncode}.')
            if verbose or process.returncode != 0:
                print(log.strip())
        self._clients = []
        return succeeded


def ReadUntil(transport, predicate, timeout, transcript, observer=None):
    '''
    Accumulate console output until predicate reports the text is complete.

    Each piece of output is also handed to observer, when there is one.

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
        if observer is not None:
            observer(decoded)
        if predicate(text):
            return (text, True)
    return (text, False)


def RunSelector(transport, selector, timeout, transcript, observer=None):
    '''
    Run one selector and return its result as a tuple of status and counts.

    unity_gets() empties the receive buffer before it reads, and it does that after the
    prompt has already been sent.  A selector written while the device is still between
    the two is therefore discarded, and the run then waits out its whole timeout for a
    reply that will never come.  The prompt alone is not proof the device is ready, so
    the selector is sent again if nothing comes back; unity_gets() echoes what it
    receives, which makes any output an acknowledgement.
    '''
    mark = len(transcript)

    for attempt in range(SELECTOR_ATTEMPTS):
        transport.write((selector + '\n').encode())
        _, acknowledged = ReadUntil(transport, lambda text: text.strip() != '',
                                    SELECTOR_ACKNOWLEDGE_SECONDS, transcript, observer)
        if acknowledged:
            break
    else:
        return ('ignored', ''.join(transcript[mark:]), None)

    #
    #   The prompt is looked for across everything this selector has produced, not just
    #   what arrives from here on.  A test that finishes quickly can have its whole
    #   output, prompt included, delivered in the same read that acknowledged the
    #   selector, and a prompt that has already gone past is never seen again: the wait
    #   then lasts the full timeout and the run still reports a pass, because the summary
    #   is in the segment either way.  That made the time a run took depend on --timeout
    #   rather than on the work.
    #
    def Settled(_):
        return MENU_PROMPT in ''.join(transcript[mark:]) or BOOT_PROMPT in ''.join(transcript[mark:])

    #
    #   ReadUntil only tests the predicate after something new arrives, so the case where
    #   the prompt is already in hand has to be dealt with before it is called.
    #
    if not Settled(''):
        ReadUntil(transport, Settled, timeout, transcript, observer)

    segment = ''.join(transcript[mark:])

    if BOOT_PROMPT in segment:
        return ('rebooted', segment, None)

    match = SUMMARY.search(segment)
    if match is None:
        return ('stalled', segment, None)

    total, failures, ignored = (int(value) for value in match.groups())
    status = 'passed' if failures == 0 else 'failed'
    return (status, segment, (total, failures, ignored))


def WriteJUnitReport(path, results):
    '''
    Write the results as a JUnit report so a build pipeline can display them.

    Each selector becomes a test suite and each Unity result line within it becomes a
    test case.  A selector that stalled or rebooted has no per-test results to report,
    so it is recorded as a single failing case carrying the console output; otherwise
    the run would look empty rather than broken.
    '''
    document = minidom.Document()
    suites = document.createElement('testsuites')
    document.appendChild(suites)

    for selector, status, segment in results:
        suite = document.createElement('testsuite')
        suite.setAttribute('name', selector)
        suites.appendChild(suite)

        cases = UNITY_RESULT.findall(segment)
        if not cases:
            #
            #   A test that restarts the device on purpose never prints a result line,
            #   so it is recorded as the one thing that did happen rather than as a
            #   failure.
            #
            case = document.createElement('testcase')
            case.setAttribute('name', selector)
            case.setAttribute('classname', 'MeadowComms')
            if status != 'rebooted as expected':
                failure = document.createElement('failure')
                failure.setAttribute('message', f'Selector {status}')
                failure.appendChild(document.createTextNode(segment[-4000:]))
                case.appendChild(failure)
            suite.appendChild(case)
            suite.setAttribute('tests', '1')
            suite.setAttribute('failures', '0' if status == 'rebooted as expected' else '1')
            continue

        failed = 0
        for fileName, lineNumber, name, result, message in cases:
            case = document.createElement('testcase')
            case.setAttribute('name', name)
            case.setAttribute('classname', os.path.basename(fileName))
            case.setAttribute('file', fileName)
            case.setAttribute('line', lineNumber)
            if result == 'FAIL':
                failed += 1
                failure = document.createElement('failure')
                failure.setAttribute('message', message.strip() or 'Test failed')
                case.appendChild(failure)
            elif result == 'IGNORE':
                case.appendChild(document.createElement('skipped'))
            suite.appendChild(case)
        suite.setAttribute('tests', str(len(cases)))
        suite.setAttribute('failures', str(failed))

    with open(path, 'w') as handle:
        handle.write(document.toprettyxml(indent='  '))


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
    parser.add_argument('--timeout', type=int, default=600,
                        help='Seconds to allow each selector (default 600)')
    parser.add_argument('--fresh', action='store_true',
                        help='Reset to a known state before running')
    parser.add_argument('--expect-reboot', action='append', default=[],
                        metavar='SELECTOR',
                        help='Selector that restarts the device on purpose; the run '
                             'continues with the next selector rather than stopping. '
                             'May be given more than once')
    parser.add_argument('--transcript', default=None,
                        help='Write the full console transcript to this file')
    parser.add_argument('--junit', default=None,
                        help='Write the results as a JUnit report to this file')
    parser.add_argument('--verbose', action='store_true',
                        help='Echo the console output as tests run')
    parser.add_argument('--bluetooth-python', default=sys.executable,
                        help='Interpreter, with bleak installed, that runs the Bluetooth '
                             'clients (default: the one running this script)')
    parser.add_argument('--soak-iterations', type=int, default=DEFAULT_SOAK_ITERATIONS,
                        help=f'Values the Bluetooth soak client writes '
                             f'(default {DEFAULT_SOAK_ITERATIONS})')
    arguments = parser.parse_args()

    if arguments.soak_iterations < 1:
        parser.error('--soak-iterations must be at least 1')

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
    results = []
    launcher = ClientLauncher(arguments.bluetooth_python, arguments)
    try:
        _, booted = ReadUntil(transport, lambda text: BOOT_PROMPT in text, 60, transcript)
        if not booted:
            print('The test menu was never reached.')
            print(''.join(transcript)[-2000:])
            return 2

        for selector in arguments.selectors:
            print(f'\n=== {selector} ===')
            result, segment, counts = RunSelector(transport, selector,
                                                  arguments.timeout, transcript,
                                                  launcher.Observe)
            clientsSucceeded = launcher.Finish(arguments.verbose)

            #
            #   A client that fails ends without the stop value, so the device test fails
            #   as well.  This catches the case where it does not, rather than letting a
            #   client that never really ran pass unnoticed.
            #
            if (result == 'passed') and not clientsSucceeded:
                result = 'failed'

            if result == 'rebooted' and selector in arguments.expect_reboot:
                result = 'rebooted as expected'

            results.append((selector, result, segment))
            if arguments.verbose or result in ('stalled', 'rebooted', 'ignored'):
                print(segment.strip())

            if result == 'rebooted as expected':
                #
                #   The device is back at the boot prompt, which is where the run
                #   started, so the next selector can be offered straight away.
                #
                print(f'{selector}: the device rebooted, as expected.')
                continue

            if result == 'rebooted':
                print(f'{selector}: the device rebooted part way through.')
                status = 3
                break
            if result == 'ignored':
                print(f'{selector}: the device never acknowledged the selector.')
                status = 2
                break
            if result == 'stalled':
                print(f'{selector}: no result within {arguments.timeout}s.')
                status = 2
                break

            total, failures, ignored = counts
            print(f'{selector}: {total} tests, {failures} failures, {ignored} ignored.')
            if (result == 'failed') and (status == 0):
                status = 1
    finally:
        if arguments.transcript:
            with open(arguments.transcript, 'w') as handle:
                handle.write(''.join(transcript))
        if arguments.junit:
            WriteJUnitReport(arguments.junit, results)
        transport.close()

    return status


if __name__ == '__main__':
    sys.exit(main())
