#!/usr/bin/env python3

from datetime import datetime
import argparse
import sys
from MessageDispatcher import MessageDispatcher
from MeadowTestController import MeadowTestController
from DeviceInformation import DeviceInformation
from MeadowTests.MeadowTests import MeadowTests

#
#   Setup the parser and parse the command line.
#
parser = argparse.ArgumentParser(description='Control the execution of the tests on a Meadow board.')
parser.add_argument('-s', '--serial', metavar='port', type=str, help='name of the serial port to open.', default = None)
parser.add_argument('-l', '--log', help=f'generate a log file?', action='store_true', default = False)
parser.add_argument('-v', '--verbose', help='print the commands that would be executed (default: False)', action='store_true', default = False)
group = parser.add_mutually_exclusive_group()
group.add_argument('-t', '--tests', nargs='*', metavar='test', help='execute the specified test(s)', default = None)
group.add_argument('-f', '--file', nargs=1, metavar='file', help=f'script file to be executed', default = None)
arguments = parser.parse_args()

#
#   now run the tests.
#
print('Meadow WiFi Test Runner')
try:
    start = datetime.now()
    if arguments.log:
        log_file_name = datetime.now().strftime('%Y%m%d-%H%M%S-MeadowTestResults.txt')
    else:
        log_file_name = None
    test_controller = MeadowTestController(arguments.serial, arguments.verbose, log_file_name)
    if test_controller.message_dispatcher is not None:
        test_controller.log(f'Test start: {datetime.now().strftime("%d %B %Y %H:%M:%S")}', True)
        device_information = DeviceInformation(test_controller)
        device_information.populate_from_meadow()
        test_controller.log(device_information.to_string(), True)
        if (arguments.tests is None) and (arguments.file is None):
            test_controller.run_all_tests()
        elif arguments.file is None:
            test_controller.run_tests(arguments.tests)
        else:
            test_controller.run_script_file(f'Scripts/{arguments.file[0]}')
        test_controller.execute_command('meadow mono disable')
        test_controller.log(f'Test end: {datetime.now().strftime("%d %B %Y %H:%M:%S")}', True)
    else:
        print('Cannot create test controller.')
    hours, remainder = divmod((datetime.now() - start).seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours > 0:
        test_controller.log(f'Elapsed time: {hours} hours {minutes} minutes {seconds} seconds', True)
    else:
        test_controller.log(f'Elapsed time: {minutes} minutes {seconds} seconds', True)
    test_controller = None
except KeyboardInterrupt:
    print('\nExecution aborted by user')
    try:
        sys.exit(0)
    except SystemExit:
        pass

