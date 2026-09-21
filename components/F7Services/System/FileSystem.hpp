/**
 * @file FileSystem.hpp
 * @author Mark Stevens
 * @brief Implementation of the FileSystem class for working with files on the ESP32.
 * @date 2023-01-08
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include "esp_littlefs.h"

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"

class FileSystem
{
private:
    /**
     *  @brief Object that will be used to send messages to the STM32 from the ESP32.
     */
    IMessageDispatcher *_messageDispatcher;

    /**
     * @brief LittleFS file system configuration.
     */
    esp_vfs_littlefs_conf_t _config;

    /**
     * @brief Private File System constructor.
     */
    FileSystem();

    /**
     * @brief Mount the file system
     *
     * @return true If file system mounted.
     * @return false If the file system could not be mounted.
     */
    bool Mount();

    /**
     * @brief Mount the file system if necessary.
     *
     * @return true If the file system is mounted (or was mounted successfully)
     * @return false If the file system could not be mounted.
     */
    bool MountIfNecessary();

    /**
     * @brief Display the list of files on the file system to the console.
     */
    void ShowFiles();

    /**
     * @brief Reject a file name that could escape the littlefs mount point or that the
     *  file system cannot store.
     *
     *  name is rejected if it is null or empty, contains a path separator ('/' or '\\') or
     *  the substring "..", or is not shorter than CONFIG_LITTLEFS_OBJ_NAME_LEN (the
     *  longest name, including the NUL terminator, the mounted file system can hold).  No
     *  separator is ever permitted, so every accepted name is a single path component and
     *  the path built from it can never resolve outside PARTITION_PATH_NAME.
     *
     * @param name
     *      Candidate file name, typically supplied by the STM32.
     *
     * @returns
     *      true if name is safe to concatenate onto PARTITION_PATH_NAME.
     */
    bool IsValidFileName(const char * const name);

    /**
     * @brief Turn a request into its response and hand it to the dispatcher.
     *
     *  message is recycled as its own response, so this only updates its type and status
     *  code before queuing it.  _messageDispatcher is only ever null in tests that build a
     *  FileSystem directly rather than through the normal message-handling path, but a
     *  response cannot be delivered without one; when that happens, message is logged and
     *  released here instead of being handed to a null pointer.
     *
     * @param message
     *      Request being turned into its own response.
     *
     * @param result
     *      Status code the response should carry.
     */
    void SendResponse(Message * const message, const StatusCodes::StatusCodes result);

public:
    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Name of the partition in the partition table.
     *
     *  Note that this can be different from the PARTITION_LABEL.
     */
    static const char *PARTITION_PATH_NAME;

    /**
     * @brief Label for the Little file system.
     */
    static const char *PARTITION_LABEL;

    /**
     * @brief Public File System constructor.
     */
    FileSystem(IMessageDispatcher *);

    /**
     * @brief Destroy the File System object
     */
    ~FileSystem();

    /**
     *  @brief Perform any class level setup.
     */
    StatusCodes::StatusCodes Setup();

    /**
     * @brief Mount the file system.
     *
     * @return true If the file system is mounted OK.
     * @return false If the file is not mounted.
     */
    bool IsMounted();

    /**
     * @brief Format the file system.
     */
    void Format(Message * const message);

    /**
     * @brief Read a file from the file system.
     */
    void ReadFile(Message * const message);

    /**
     * @brief Write a file to the file system.
     */
    void WriteFile(Message * const message);

    /**
     * @brief Delete a file from the file system.
     */
    void DeleteFile(Message * const message);

    /**
     * @brief Get a list of the files on the file system.
     */
    void ListFiles(Message * const message);
};