/*
 *  FileSystem.hpp
 *
 *  File system message request handlers
 */

#ifndef _FILE_SYSTEM_HPP_
#define _FILE_SYSTEM_HPP_

#include "Esp32Messaging.hpp"

class FileSystem
{
private:
    /**
     *  @brief Object that will be used to send messages to the STM32 from the ESP32.
     */
    MessageDispatcher *_messageDispatcher;

    /**
     * @brief LittleFS file system configuration.
     */
    esp_vfs_littlefs_conf_t _config;

    /**
     * @brief Private File System constructor.
     */
    FileSystem();

    /**
     * @brief Destroy the File System object
     */
    ~FileSystem();

public:
    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Public File System constructor.
     */
    FileSystem(MessageDispatcher *);

    /**
     *  @brief Perform any class level setup.
     */
    StatusCodes::StatusCodes Setup();
    
    /**
     * @brief Format the file system.
     */
    void Format(Message *);

    /**
     * @brief Read a file from the file system.
     */
    void ReadFile(Message *);

    /**
     * @brief Write a file to the file system.
     */
    void WriteFile(Message *);

    /**
     * @brief Delate a file from the file system.
     */
    void DeleteFile(Message *);

    /**
     * @brief Get a list of the files on the file system.
     */
    void ListFiles(Message *);
};

#endif /* _FILE_SYSTEM_HPP_ */