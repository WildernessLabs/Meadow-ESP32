#!/usr/bin/env python3

import time

class DeviceInformation:
    '''
    Provide an internal abstraction for the device information.
    '''

    def __init__(self, controller):
        '''Construct a new DeviceInformation object.'''
        self.controller = controller
        self.information = { }

    @property
    def device_name(self):
        '''Get the name as specified in the configuration file on the Meadow board.'''
        return(self.information['DeviceName'])

    @property
    def microcontroller_id(self):
        '''Get the ID ot the STM32 processor.'''
        return(self.information['MicrocontrollerId'])

    @property
    def serial_number(self):
        '''Get the serial number of the STM32 processor'''
        return(self.information['MicrocontrollerSerialNumber'])

    @property
    def os_version(self):
        '''Get the version of NuttX deployed to the board.'''
        return(self.information['OsVersion'])

    @property
    def model(self):
        '''Get the device model.'''
        return(self.information['Model'])

    @property
    def mono_version(self):
        '''Get the version of Mono deployed to the board.'''
        return(self.information['MonoVersion'])

    @property
    def processor(self):
        '''Get the main processor type.'''
        return(self.information['ProcessorType'])

    @property
    def product(self):
        '''Get the board type (name).'''
        return(self.information['Product'])

    @property
    def build_date(self):
        '''Get the date and time the software was built.'''
        return(self.information['BuildDate'])

    @property
    def coprocessor_type(self):
        '''Get the coprocessor type (should be ESP32)'''
        return(self.information['CoprocessorType'])

    @property
    def coprocessor_firmware_version(self):
        '''Get the version number of the firmware in the ESP32 coprocessor.'''
        return(self.information['CoprocessorFirmwareVersion'])

    @property
    def mac_address(self):
        '''Get the MAC address of the Meadow board.'''
        return(self.information['MacAddress'])

    def populate_from_meadow(self):
        '''Get the device information from the Meadow board.'''
        self.controller.reset_meadow()
        time.sleep(1.0)
        self.controller.execute_command('meadow mono enable -f')
        self.controller.wait_for_meadow_to_be_ready()
        self.controller.message_dispatcher.write(f'getdeviceinfo\n')
        done = False
        while not done:
            data = self.controller.message_dispatcher.read()
            if (len(data) != 0):
                line = data.decode().rstrip()
                if f'DeviceInfoTest End' in line:
                    done = True
                else:
                    items = line.split(': ')
                    if len(items) == 2:
                        self.information[items[0]] = items[1]
            else:
                time.sleep(0.1)

    def to_string(self):
        '''Convert the device information into a summary.'''
        if len(self.information) > 0:
            result = f'Device information: Name - {self.information["DeviceName"]}, Product - {self.information["Product"]}\n'
            result += f'OS versions: NuttX - {self.information["OsVersion"]}, Mono - {self.information["MonoVersion"]}, ESP32 - {self.information["CoprocessorFirmwareVersion"]}\n'
            result += f'Build date: {self.information["BuildDate"]}'
        else:
            result = "Device information is unavailable."
        return(result)
