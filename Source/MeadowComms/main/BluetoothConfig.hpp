/*
 *  BluetoothConfig.hpp
 *
 *  Class for storing bluetooth GATT configuration
 */

#ifndef _BLUETOOTH_CONFIG_HPP_
#define _BLUETOOTH_CONFIG_HPP_

#include <string.h>

#define MAX_NAME_LEN 128

class BluetoothConfig
{
    
private:
    char m_deviceName[MAX_NAME_LEN];

public:
    BluetoothConfig() {}
    ~BluetoothConfig() {}
    
    void SetDeviceName(char* name);
    char* GetDeviceName();
};

#endif