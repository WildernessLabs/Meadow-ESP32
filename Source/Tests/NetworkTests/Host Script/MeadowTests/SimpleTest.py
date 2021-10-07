#!/usr/bin/env python3

from MeadowTests.MeadowTestBase import MeadowTestBase

class SimpleTest(MeadowTestBase):
    '''
    Implement the methods for a simple test.
    '''

    def __init__(self, name, controller):
        '''Create a new instance of this class.'''
        self.controller = controller
        self.name = name
        MeadowTestBase.__init__(self, controller)

    def setup(self):
        '''Setup the test (the defaults in the base class are sufficient for this test).'''
        MeadowTestBase.setup(self)

    def execute(self):
        '''Execute the test (the defaults in the base class are sufficient for this test).'''
        return MeadowTestBase.execute(self, self.name)

    def teardown(self):
        '''Clean up after the test (the defaults in the base class are sufficient for this test).'''
        MeadowTestBase.teardown(self)

