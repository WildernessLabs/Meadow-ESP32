/*
 *  NvsManager.hpp
 *
 *  Provide a mechanism for storing and retrieving data in non-Volatile storage (NVS).
 */
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "NvsManager.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

NvsManager::NvsManager()
{
}

NvsManager::~NvsManager()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
/**
 *  Name of the namespace in NVS used to store Meadow data.
 */
const char *NvsManager::STORAGE_NAMESPACE_NAME = "Meadow";

/**
 *  Name of the area in the NVS that holds the system configuration.
 */
const char *SYSTEM_CONFIG_NAME = "SysConfig";

/**
 *  Name of the area in the NVS that holds the WiFi configuration.
 */
const char *WIFI_CONFIG_NAME = "WiFiConfig";

/**
 *  Name of the area in the NVS that holds the bluetooth configuration.
 */
const char *BLUETOOTH_CONFIG_NAME = "BTConfig";

/**
 *  Name of the area in the NVS that holds the mesh network configuration.
 */
const char *MESH_NETWORK_CONFIG_NAME = "MeshConfig";

/**
 *  Determine if this object has been created already.
 */
bool NvsManager::_nvsAvailable = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the non-volatile storage on the ESP32.
 */
void NvsManager::Setup()
{
    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_LOGI(__func__, "Formatting NVS.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    _nvsAvailable = (result == ESP_OK);
}

/**
 *  Read a byte (uint8_t) from non-volatile storage.
 */
NvsManager::ErrorCodes NvsManager::ReadUint8(const char *name, uint8_t *value)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(STORAGE_NAMESPACE_NAME, NVS_READONLY, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_get_u8(nvsHandle, name, value);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
        }
        nvs_close(nvsHandle);
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

/**
 *  Write a byte (uint8_t) to non-volatile storage.
 */
NvsManager::ErrorCodes NvsManager::WriteUint8(const char *name, uint8_t value)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(STORAGE_NAMESPACE_NAME, NVS_READONLY, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_set_u8(nvsHandle, name, value);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            if (result == ErrorCodes::Ok)
            {
                error = nvs_commit(nvsHandle);
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
        }
        nvs_close(nvsHandle);
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

/**
 *  Read a boolean from non-volatile storage.
 */
NvsManager::ErrorCodes NvsManager::ReadBoolean(const char *name, bool *value)
{
    NvsManager::ErrorCodes result;
    uint8_t byte;

    result = ReadUint8(name, &byte);
    if (result == ErrorCodes::Ok)
    {
        *value = (byte == 0) ? false : true;
    }
    return(result);
}

/**
 *  Write a boolean to non-volatile storage.
 */
NvsManager::ErrorCodes NvsManager::WriteBoolean(const char *name, bool value)
{
    uint8_t byte;

    byte = value ? 1 : 0;
    return(WriteUint8(name, byte));
}

/**
 *  @brief Obtain the amount of storage (in NVS) used by the named BLOB.
 *
 *  @param name
 *      Name of the BLOB stored in NVS.
 *
 *  @param spaceRequired
 *      Pointer to a size_t object that will be used to store the space required
 *      in NVS for the anmed BLOB.
 *
 *  @returns
 *      ErroCodes object indicating the success or failure of this mehtod call.
 */
NvsManager::ErrorCodes NvsManager::BlobSize(const char *name, size_t *spaceRequired)
{
    nvs_handle nvsHandle;
    esp_err_t error;

    if (_nvsAvailable)
    {
        return(ErrorCodes::NotAvailable);
    }

    error = nvs_open(STORAGE_NAMESPACE_NAME, NVS_READONLY, &nvsHandle);
    if (error != ESP_OK)
    {
        return(ErrorCodes::Error);
    }
    //
    //  First work out how much space is required.
    //
    error = nvs_get_blob(nvsHandle, name, NULL, spaceRequired);
    if (error == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvsHandle);
        return(ErrorCodes::NotFound);
    }
    if (error != ESP_OK)
    {
        nvs_close(nvsHandle);
        return(ErrorCodes::Error);
    }
    nvs_close(nvsHandle);
    return(ErrorCodes::Ok);
}

/**
 *  @brief Read a block of memory from non-volatile storage.
 *
 *  @param location
 *      Block of memory to populate with the data from non-volatile storage.
 *
 *  @param amount
 *      Number of bytes to read.
 *
 *  @retruns
 *      true if successful, false otherwise.
 */
NvsManager::ErrorCodes NvsManager::ReadBlob(const char *name, void *location, size_t amount)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    ErrorCodes result;

    if (_nvsAvailable)
    {
        return(ErrorCodes::NotAvailable);
    }
    //
    //  First work out how much space is required.
    //
    size_t spaceRequired = 0;
    result = BlobSize(name, &spaceRequired);
    if (result != ErrorCodes::Ok)
    {
        return(result);
    }
    if (amount != spaceRequired)
    {
        return(ErrorCodes::BufferSizeMismatch);
    }
    //
    //  We know we have enough storage so read the BLOB.
    //
    error = nvs_open(STORAGE_NAMESPACE_NAME, NVS_READONLY, &nvsHandle);
    if (error != ESP_OK)
    {
        return(ErrorCodes::Error);
    }
    error = nvs_get_blob(nvsHandle, name, location, &spaceRequired);
    nvs_close(nvsHandle);

    return((error == ESP_OK) ? ErrorCodes::Ok : ErrorCodes::Error);
}

/**
 *  @brief Read a block of memory from non-volatile storage.
 *
 *  @param location
 *      Block of memory to write to non-volatile storage.
 *
 *  @param amount
 *      Number of bytes to write.
 *
 *  @retruns
 *      Error code representing the result of the write request.
 */
NvsManager::ErrorCodes NvsManager::WriteBlob(const char *name, void *location, size_t amount)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(STORAGE_NAMESPACE_NAME, NVS_READONLY, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_set_blob(nvsHandle, name, location, amount);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            if (result == ErrorCodes::Ok)
            {
                error = nvs_commit(nvsHandle);
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
        }
        nvs_close(nvsHandle);
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

