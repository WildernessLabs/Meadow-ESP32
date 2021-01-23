#include "BluetoothConfig.hpp"

void BluetoothConfig::SetDeviceName(char* name)
{
    memset(m_deviceName, 0, MAX_NAME_LEN);
    strncpy(m_deviceName, name,  strnlen(name, MAX_NAME_LEN));
}

char* BluetoothConfig::GetDeviceName()
{
    return m_deviceName;
}
