#!/usr/bin/env python3

import serial
import platform

class MessageDispatcher:
    '''
    Act as a way to send / receive messages to a serial port.
    '''
    
    def __init__(self, port_name = None):
        '''
        Parameters
        ----------
        port_name  : str
            Name of the serial port connected to the Meadow board.
        '''
        if port_name is None:
            if platform.uname().system == 'Darwin':
                port_name = '/dev/tty.usbserial-0001'
            else:
                port_name = '/dev/ttyS0'
        try:
            self.meadow = serial.Serial(port = port_name, baudrate = 115200, parity = serial.PARITY_NONE, stopbits = serial.STOPBITS_ONE, bytesize = serial.EIGHTBITS, timeout = 1)        
            self.meadow.flushInput()
        except:
            self.meadow = None

    def __del__(self):
        '''Dispose of any resources used by an instance of this class.'''
        if self.meadow is not None:
            self.meadow.close()

    def has_serial_port(self):
        '''Indicate if the serial port is available.'''
        return(self.meadow is not None)

    def write(self, data):
        '''Send the data to the serial port.
        
        Parameters
        ----------
        data : str
            Data to be sent to the Meadow board.
        '''
        if (self.has_serial_port() or self.arguments.dryrun):
            if (self.has_serial_port):
                self.meadow.write(data.encode())
                self.meadow.flushOutput()
            else:
                print(f'    Sending data: {data}')

    def read(self):
        '''Read a line of data from the Meadow board.'''
        if (self.has_serial_port() or self.arguments.dryrun):
            if (self.has_serial_port):
                data = self.meadow.readline()
            else:
                data = input(f'    Enter response: ')
        return(data)

    def flush(self):
        '''Flush the input and output buffers.'''
        self.meadow.flushInput()
        self.meadow.flushOutput()

if (__name__ == '__main__'):
    dispatcher = MessageDispatcher('BadPortName')
    if dispatcher.has_serial_port():
        print('FAIL: Bad serial port name.')
    else:
        print('PASS: Bad serial port has not been opened.')
    dispatcher = MessageDispatcher()
    if dispatcher.has_serial_port():
        print('PASS: Default serial port opened.')
    else:
        print('FAIL: Opening default serial port.')
    dispatcher.write('Test message.')
    text = dispatcher.read()
    print(f'Received: "{text}"')
