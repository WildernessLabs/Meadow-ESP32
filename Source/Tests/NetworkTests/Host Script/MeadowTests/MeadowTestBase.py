#!/usr/bin/env python3

import time

class MeadowTestBase:
    '''
    Base class for the Meadow tests.
    '''
    
    def __init__(self, controller):
        '''Create a new instance of the base class.'''
        self.controller = controller

    def setup(self):
        '''
        Setup the board and environment for the test.
        '''
        self.controller.reset_meadow()
        time.sleep(1.0)
        self.controller.execute_command('meadow mono enable -f')
        self.controller.wait_for_meadow_to_be_ready()

    def execute(self, test_name):
        '''
        Execute the names test.

        Parameters
        ----------
        test_name : str
            Name of the test to be executed on the Meadow board.
        '''
        self.controller.message_dispatcher.write(f'executetests {test_name}\n')
        done = False
        while not done:
            data = self.controller.message_dispatcher.read()
            if (len(data) != 0):
                line = data.decode().rstrip()
                if f'{test_name} PASS' in  line:
                    passed = True
                elif f'{test_name} FAIL' in line:
                        passed = False
                        self.controller.log(f'    {line}', True)
                if f'{test_name} End' in line:
                    done = True
                if line.startswith('LOG:') or self.controller.verbose:
                    self.controller.log(f'    {line}', True)
            else:
                time.sleep(0.1)
        return(passed)

    def teardown(self):
        '''Clean up after executing the test.'''
        pass
