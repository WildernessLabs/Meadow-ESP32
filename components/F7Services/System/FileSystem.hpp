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

public:
    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Name of the partition in the partition table.
     *
     *  Note that this can be different from the pARTITION_LABEL.
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
     * @brief Delate a file from the file system.
     */
    void DeleteFile(Message * const message);

    /**
     * @brief Get a list of the files on the file system.
     */
    void ListFiles(Message * const message);
};