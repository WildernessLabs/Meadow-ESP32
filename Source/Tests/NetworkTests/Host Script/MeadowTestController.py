#!/usr/bin/env python3
import time
import datetime
import os
import sys
import subprocess
import shlex
import platform
from MessageDispatcher import MessageDispatcher
from MeadowTests import MeadowTests

class MeadowTestController:
    '''
    Run one or more tests on the Meadow board.        
    '''

    def __init__(self, port_name = None, verbose = False, log_file = None):
        '''Create a new MeadowTestController object'''
        self.meadow = None
        if log_file is not None:
            self.log_file = open(log_file, 'wt')
        else:
            self.log_file = None
        self.verbose = verbose
        self.message_dispatcher = MessageDispatcher(port_name)
        if platform.uname().system == 'Darwin':
            self.reset_pin = None
        else:
            import board
            import digitalio
            self.reset_pin = digitalio.DigitalInOut(board.D13)
            self.reset_pin.direction = digitalio.Direction.OUTPUT
            self.reset_pin.value = False        # Hold the reset pin low until the meadow board is required.

    def __del__(self):
        '''Delete the object releasing any resources.'''
        if (self.log_file is not None):
            self.log_file.close()

    def execute_command(self, command):
        '''Execute an OS command to control the Meadow upload process.'''
        if self.verbose:
            print(f'Executing command: {command}')
        output = subprocess.check_output(shlex.split(command))
        if self.verbose:
            lines = output.decode('utf-8').split('\n')
            for line in lines:
                print(f'    {line}')

    def wait_for_meadow_to_be_ready(self):
        '''Wait for the Meadow board to be ready to accept commands.'''
        meadow_ready = False
        while not meadow_ready:
            data = self.message_dispatcher.read()
            if (len(data) != 0):
                line = data.decode().rstrip()
                if (line == "Ready"):
                    meadow_ready = True
                else:
                    time.sleep(0.1)

    def reset_meadow(self):
        '''Reset the Meadow board by:
        
        Pi: Pulling the reset GPIO low for 100ms
        MacOS: Use STM32 Cube Programmer to reset the board through a connected ST-Link.

        Parameters:
        wait : bool
            Indicate if the system should wait for the Ready response from the Meadow board.
        '''
        if platform.uname().system == 'Darwin':
            self.execute_command(f'{os.environ.get("CUBE_APP")} -c port=swd -hardRst')
        else:
            self.reset_pin.value = False
            time.sleep(0.1)
            self.reset_pin.value = True
            time.sleep(0.1)

    def setup_log_file(self, enable_logging, log_file_name):
        '''Setup the log file if required.
        
        Parameters
        ----------
        enable_logging : bool
            Turn logging to a log file on?

        log_file_name : str
            Name of the log file to be generated.
        '''
        log_file = None
        if (enable_logging):
            log_file = open(log_file_name, 'wt')
        return(log_file)

    def log(self, message, output_to_console = False):
        '''Produce a log message

        The message to be logged will be prepended with the current time before being
        printed and possibly written to the log file.

        Parameters
        ----------
        message : str
            Message to be logged.
        '''
        lines = message.split('\n')
        for line in lines:
            text = time.strftime('[%H:%M:%S]: ') + line.replace('\n', '')
            if self.verbose or output_to_console:
                print(text)
            if (self.log_file is not None):
                self.log_file.write(text + '\n')

    def log_summary(self, passed, failed):
        '''
        Work out the statistics for the run pass rate and log the detailed.

        Parameters
        ----------
        passed : list of str
            Names of the runs that were successful.

        failed : list of str
            Names of the runs that failed.
        '''
        executed = len(passed) + len(failed)
        if executed == 0:
            pass_rate = 0
        else:
            pass_rate = (len(passed) / executed) * 100
        self.log(f'Tests complete, pass rate {pass_rate:3.0f}%, {len(passed)} of {executed}', True)

    def run_all_tests(self):
        '''Run all of the known tests in the catalogue'''
        print('Running all tests.')
        passed = []
        failed = []
        for test in MeadowTests.MeadowTests(self):
            print(f'Found: {test.name}')
            self.log(f'Executing test {test.name}', True)
            test.setup()
            if test.execute():
                self.log(f'    PASS', True)
                passed.append(test.name)
            else:
                self.log(f'    FAIL', True)
                failed.append(test.name)
            test.teardown()
        self.log_summary(passed, failed)

    def run_tests(self, test_list):
        '''
        Execute one or more tests on the Meadow board.
        '''
        self.log('Executing tests.', True)
        passed = []
        failed = []
        if (test_list is not None):
            tests = MeadowTests.MeadowTests(self)
            for test_name in test_list:
                test = tests.find_test(test_name)
                if test is not None:
                    self.log(f'Executing test {test_name}', True)
                    test.setup()
                    if test.execute():
                        self.log(f'    PASS', True)
                        passed.append(test_name)
                    else:
                        self.log(f'    FAIL', True)
                        failed.append(test_name)
                    test.teardown()
        self.log_summary(passed, failed)

    def run_script_file(self, file_name):
        '''
        Execute a number of tests using a script file to control the test run.

        It is assumed that the script file contains a number of lines that are either
        comments (lines that start with a #), a test name or a test name followed by
        the number of times the test should be executed.

        e.g.
        
        # This is a comment.
        # The following line contains a test that should be run once:
        Test1
        # The following line contains a test that should be run five times:
        Test2 5
        
        Parameters
        ----------
        file_name : str
            Name of the script file.
        '''
        self.log(f'Executing tests from file {file_name}', True)
        tests = MeadowTests.MeadowTests(self)
        passed = []
        failed = []
        with open(file_name, 'rt') as script:
            line_number = 0
            for line in script:
                line_number += 1
                if not (line.startswith('#') or (len(line.rstrip()) == 0)):
                    actions = line.rstrip().split(' ')
                    if len(actions) == 1:
                        amount = 1
                    else:
                        try:
                            amount = int(actions[1])
                        except:
                            amount = 1                    
                    test = tests.find_test(actions[0])
                    if test is not None:
                        for run in range(amount):
                            if amount > 1:
                                test_name = f'{test.name}, run {run + 1} of {amount}'
                            else:
                                test_name = f'{test.name}'
                            self.log(f'Executing test {test_name}', True)
                            test.setup()
                            if test.execute():
                                self.log(f'    PASS', True)
                                passed.append(test_name)
                            else:
                                self.log(f'    FAIL', True)
                                failed.append(test_name)
                            test.teardown()
                    else:
                        self.log(f'ERROR: Line {line_number}, unknown test: {line.rstrip()}', True)

        self.log_summary(passed, failed)

if (__name__ == '__main__'):
    try:
        test_controller = MeadowTestController(None, True, None)
        start = datetime.datetime.now()
        if test_controller.message_dispatcher is not None:
            test_controller.run_all_tests()
        else:
            print('Serial connection to the Meadow board cannot be established.')
        elapsed = datetime.datetime.now() - start
        print(f'Elapsed time: {elapsed.seconds} seconds')
    except KeyboardInterrupt:
        print('Execution aborted by user')
        try:
            sys.exit(0)
        except SystemExit:
            pass
