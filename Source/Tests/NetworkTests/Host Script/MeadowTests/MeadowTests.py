#!/usr/bin/env python3

from MeadowTests.SimpleTest import SimpleTest

class MeadowTests:
    '''
    The MeadowTests class holds the list of known tests and allows the caller to
    iterate over the known tests or to find a specific instance of a test.

    The system considers two test cases:
        - Simple tests
        - Complex tests

    A simple test, represented by the SimpleTest class, simply holds the name of
    the test and executes the methods in the MeadowTestBase class.

    A complex test, represented by a specific class object, requires the test
    setup, execute or teardown to offer more functionality than is offerred by
    a simple test.
    '''

    def __init__(self, controller):
        '''Create a new MeadowTests object.'''
        self.controller = controller
        #
        #   All of the test cases are in this dictionary.  We start with a dictionary
        #   of known complex test cases.
        #
        self.test_dictionary = { }
        #
        #   Now add the simple tests to the dictionary of tests.
        #
        simple_test_names = [ 'StartWiFiInterfaceTest', 'ConnectToAccessPointTest', 'ConnectToAccessPointEventsTest', 
                              'DisconnectFromAccessPointEventsTest', 'OauthTest', 'NtpTest', 'HttpRequestTest',
                              'HttpsRequestTest', 'DeviceInfoTest' ]
        for test_name in simple_test_names:
            self.test_dictionary[test_name] = SimpleTest(test_name, controller)
        #
        #   The test is created to aid iterating over the known test cases.
        #
        self.tests = list(self.test_dictionary.values())

    def __iter__(self):
        '''Start the iteration process.'''
        self.current_item = 0
        return(self)

    def __next__(self):
        '''Get the next item in the iteration.'''
        if self.current_item < len(self.tests):
            item = self.tests[self.current_item]
            self.current_item += 1
            return(item)
        else:
            raise StopIteration

    def find_test(self, name):
        '''Try to locate a named test in the dictionary of known tests.'''
        try:
            item = self.test_dictionary[name]
        except:
            item = None
        return item
