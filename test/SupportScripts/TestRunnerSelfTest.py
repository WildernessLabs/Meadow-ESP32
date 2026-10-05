#!/usr/bin/env python

'''
Tests of the selector handling in TestRunner.py, against a console that is simulated.

Run with: python3 TestRunnerSelfTest.py
'''

import unittest

import TestRunner


class SimulatedConsole(TestRunner.Transport):
    '''
    A console that answers each write from a script.

    Before the first write it prints whatever background output it was given, as the SPI
    synchronisation task does.  Each reply in replies is then printed in answer to the
    write with the same index.
    '''

    def __init__(self, background, replies):
        self._pending = [background]
        self._replies = list(replies)
        self.writes = []

    def read(self):
        return self._pending.pop(0).encode() if self._pending else b''

    def write(self, data):
        self.writes.append(data.decode())
        if self._replies:
            self._pending.append(self._replies.pop(0))

    def close(self):
        pass


PASSED = '[Selected]\r\n1 Tests 0 Failures 0 Ignored\r\nOK\r\n' + TestRunner.MENU_PROMPT + '\r\n'


class SelectorTests(unittest.TestCase):

    def setUp(self):
        self._attempts = TestRunner.SELECTOR_ATTEMPTS
        self._seconds = TestRunner.SELECTOR_ACKNOWLEDGE_SECONDS
        TestRunner.SELECTOR_ACKNOWLEDGE_SECONDS = 0.2

    def tearDown(self):
        TestRunner.SELECTOR_ATTEMPTS = self._attempts
        TestRunner.SELECTOR_ACKNOWLEDGE_SECONDS = self._seconds

    def test_a_control_message_is_not_the_echo_of_a_selector(self):
        # The device drops the first selector and keeps printing +++RDY; the second is echoed.
        console = SimulatedConsole('+++RDY\r\n', ['+++RDY\r\n', '[Selected]\r\n' + PASSED])
        status, _, counts = TestRunner.RunSelector(console, '[Selected]', 2, [])
        self.assertEqual(['[Selected]\n', '[Selected]\n'], console.writes, 'The selector was not sent again.')
        self.assertEqual('passed', status)
        self.assertEqual((1, 0, 0), counts)

    def test_a_control_message_inside_the_echo_is_ignored(self):
        console = SimulatedConsole('', ['[Sel+++RDY\r\nected]\r\n' + PASSED])
        status, _, _ = TestRunner.RunSelector(console, '[Selected]', 2, [])
        self.assertEqual(['[Selected]\n'], console.writes)
        self.assertEqual('passed', status)

    def test_a_selector_only_answered_by_control_messages_is_ignored(self):
        TestRunner.SELECTOR_ATTEMPTS = 3
        console = SimulatedConsole('', ['+++RDY\r\n'] * 3)
        status, _, _ = TestRunner.RunSelector(console, '[Selected]', 1, [])
        self.assertEqual('ignored', status)
        self.assertEqual(3, len(console.writes))


if __name__ == '__main__':
    unittest.main()
