/*
 *  NvsManager.hpp
 *
 *  Provide a mechanism for storing and retrieving data in non-Volatile storage (NVS).
 */

#ifndef _NVS_MANAGER_HPP_
#define _NVS_MANAGER_HPP_

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

class NvsManager
{
public:
    /**
     *  Error codes for the configuration manager.
     */
    enum ErrorCodes { Ok, Error, NotAvailable, NotFound, BufferSizeMismatch };

private:
    /**
     *  Namespace in the NVS for Meadow data.
     */
    static const char *STORAGE_NAMESPACE_NAME;

    /**
     *  Data types that can be written by this class.
     */
    enum DataTypes { UInt8, Boolean, Blob };

    /**
     *  Indicate if the NVS memory can be used.
     */
    static bool _nvsAvailable;

public:
    /**
     *  Name of the area in the NVS that holds the system configuration.
     */
    static const char *SYSTEM_CONFIG_NAME;

    /**
     *  Name of the area in the NVS that holds the WiFi configuration.
     */
    static const char *WIFI_CONFIG_NAME;

    /**
     *  Name of the area in the NVS that holds the bluetooth configuration.
     */
    static const char *BLUETOOTH_CONFIG_NAME;

    /**
     *  Name of the area in the NVS that holds the mesh network configuration.
     */
    static const char *MESH_NETWORK_CONFIG_NAME;

    /**
     *  Default constructor.
     */
    NvsManager();

    /**
     *  Destructor.
     */
    virtual ~NvsManager();

    /**
     *  Setup the non-volatile storage.
     */
    static void Setup();

    /**
     *  Read a byte (uint8_t) from non-volatile storage.
     */
    static ErrorCodes ReadUint8(char *, uint8_t *);

    /**
     *  Write a byte (uint8_t) to non-volatile storage.
     */
    static ErrorCodes WriteUint8(char *, uint8_t);

    /**
     *  Read a boolean from non-volatile storage.
     */
    static ErrorCodes ReadBoolean(char *, bool *);

    /**
     *  Write a boolean to non-volatile storage.
     */
    static ErrorCodes WriteBoolean(char *, bool);

    /**
     *  Obtain the amount of storage used by the named blob.
     */
    static ErrorCodes BlobSize(char *, size_t *);

    /**
     *  Read a block of memory from non-volatile storage.
     */
    static ErrorCodes ReadBlob(char *, void *, size_t);

    /**
     *  Write a block of memory to non-volatile storage.
     */
    static ErrorCodes WriteBlob(char *, void *, size_t);
};

#endif /* _NVS_MANAGER_HPP_ */
