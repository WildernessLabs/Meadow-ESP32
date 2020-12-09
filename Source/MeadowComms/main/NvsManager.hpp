/*
 *  NvsManager.hpp
 *
 *  Provide a mechanism for storing and retrieving data in Non-Volatile Storage (NVS).
 */

#ifndef _NVS_MANAGER_HPP_
#define _NVS_MANAGER_HPP_

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/**
 *  @brief Manage the non-volatile storage.
 */
class NvsManager
{
public:
    /**
     *  @brief Error codes for the configuration manager.
     */
    enum ErrorCodes { Ok, Error, NotAvailable, NotFound, BufferSizeMismatch };

private:
    /**
     *  @brief Indicate if the NVS memory can be used.
     */
    static bool _nvsAvailable;

public:
    /**
     *  D@brief efault constructor.
     */
    NvsManager();

    /**
     *  @brief Destructor.
     */
    virtual ~NvsManager();

    /**
     *  @brief Setup the non-volatile storage.
     */
    static void Setup();

    /**
     *  @brief Read a byte (uint8_t) from non-volatile storage.
     */
    static ErrorCodes ReadUint8(const char *, uint8_t *);

    /**
     *  @brief Write a byte (uint8_t) to non-volatile storage.
     */
    static ErrorCodes WriteUint8(const char *, uint8_t);

    /**
     *  @brief Get a string value.
     */
    static char *GetValue(const char *, const char *, const char *);

    /**
     *  @brief Set a string value.
     */
    static ErrorCodes SetValue(const char *, const char *, const char *);

    /**
     *  @brief Get an unsigned integer value.
     */
    static uint32_t GetValue(const char *, const char *, const uint32_t);

    /**
     *  @brief Set an unsigned integer value.
     */
    static ErrorCodes SetValue(const char *, const char *, const uint32_t);

    /**
     *  @brief Get an integer value.
     */
    static int32_t GetValue(const char *, const char *, const int32_t);

    /**
     *  @brief Set an integer value.
     */
    static ErrorCodes SetValue(const char *, const char *, const int);

    /**
     *  @brief Get a byte value.
     */
    static uint8_t GetValue(const char *, const char *, const uint8_t);

    /**
     *  @brief Set a byte value.
     */
    static ErrorCodes SetValue(const char *, const char *, const uint8_t);

    /**
     *  @brief Get a boolean value.
     */
    static bool GetValue(const char *, const char *, const bool);

    /**
     *  @brief Set a boolean value.
     */
    static ErrorCodes SetValue(const char *, const char *, const bool);
};

#endif /* _NVS_MANAGER_HPP_ */
