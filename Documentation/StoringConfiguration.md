# Storing Configuration Values

The partition table on the ESP32 has a small amount of space made available for the storage of data that will survive a power loss or reset.  This area is accessed through the `SystemRequestHandler` class.  This itself calls through to the `NvsManager` class which performs the low level access to the storage space on the ESP32 chip.

The configuration values are access through the following methods:

```C++
static char *GetConfigurationValue(const char *name, const char *defaultValue);
static uint32_t GetConfigurationValue(const char *name, const uint32_t defaultValue);
static int GetConfigurationValue(const char *name, const int32_t defaultValue);
static uint8_t GetConfigurationValue(const char *name, const uint8_t defaultValue);
static bool GetConfigurationValue(const char *name, const bool defaultValue);
static NvsManager::ErrorCodes SetConfigurationValue(const char *name, const char *defaultValue);
static NvsManager::ErrorCodes SetConfigurationValue(const char *name, const uint32_t defaultValue);
static NvsManager::ErrorCodes SetConfigurationValue(const char *name, const int32_t) defaultValue;
static NvsManager::ErrorCodes SetConfigurationValue(const char *name, const uint8_t defaultValue);
static NvsManager::ErrorCodes SetConfigurationValue(const char *name, const bool defaultValue);
```

The getters take a default value and this will be used if there is a problem access the storage space or if the value has not yet been set in the storage space.