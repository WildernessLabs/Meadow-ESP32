/*
 *  FileSystem.cpp
 *
 *  Implementation of the methods to provide the file system functionality for the STM32.
 */

#include "sdkconfig.h"

#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "FileSystem.hpp"
#include "SharedEnums.hpp"
#include "Mapping.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the FreeRTOS component.
 */
const char *SystemRequestHandler::COMPONENT_NAME = "FileSystem";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new FileSystem object (Private)
 */
FileSystem::FileSystem()
{
    _messageDispatcher = nullptr;
    _config =
    {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
}

/**
 * @brief Construct a new File System object
 * 
 * @param messageDispatcher
 *      Message dispatcher object used to queue messages for the STM32.
 */
FileSystem::FileSystem(MessageDispatcher *messageDispatcher) : FileSystem()
{
    _messageDispatcher = messageDispatcher;
}

/**
 * @brief Destroy the FileSystem object
 */
FileSystem::~FileSystem()
{
    esp_vfs_littlefs_unregister(_config.partition_label);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Getters and setters.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Mount the file system.
 * 
 * @return true If the file system is mounted OK.
 * @return false If the file is not mounted.
 */
bool FileSystem::IsMounted()
{
    return(esp_littlefs_mounted(_config.partition_label));
}

/**
 * @brief Mount the file system
 * 
 * @return true If file system mounted.
 * @return false If the file system could not be mounted.
 */
bool FileSystem::Mount()
{
    bool result = true;

    esp_err_t ret = esp_vfs_littlefs_register(&_config);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            TRACE_MESSAGE("Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            TRACE_MESSAGE("Failed to find LittleFS partition");
        }
        else
        {
            TRACE_MESSAGE("Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
    }

    return(result);
}

/**
 * @brief Mount the file system if necessary.
 * 
 * @return true If the file system is mounted (or was mounted successfully)
 * @return false If the file system could not be mounted.
 */
bool FileSystem::MountIfNecessary()
{
    bool result = IsMounted();
    if (!result)
    {
        result = Mount();
    }

    return(result);
}

StatusCodes::StatusCodes FileSystem::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    TRACE_MESSAGE("Setup: Exit");

    return(result);
}

/**
 * @brief Format the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::Format(Message *request)
{
    TRACE_MESSAGE("Format: Enter");

    esp_err_t ret;

    ret = esp_littlefs_format(_config.partition_label);
    if (ret != ESP_OK)
    {
        TRACE_MESSAGE("Filed to format the file system (%s)", esp_err_to_name(ret));
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(_config.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        TRACE_MESSAGE("Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        TRACE_MESSAGE("Partition size: total: %d, used: %d", total, used);
    }

    TRACE_MESSAGE("Format: Exit");
}

/**
 * @brief Read a file from the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::ReadFile(Message *request)
{
    TRACE_MESSAGE("ReadFile: Enter");

    TRACE_MESSAGE("ReadFile: Exit");
}

/**
 * @brief Write a file to the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::WriteFile(Message *request)
{
    TRACE_MESSAGE("WriteFile: Enter");

    TRACE_MESSAGE("WriteFile: Exit");
}

/**
 * @brief Delete a file on the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::DeleteFile(Message *request)
{
    TRACE_MESSAGE("DeleteFile: Enter");

    TRACE_MESSAGE("DeleteFile: Exit");
}

/**
 * @brief List the files on the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::ListFiles(Message *request)
{
    TRACE_MESSAGE("ListFiles: Enter");

    TRACE_MESSAGE("ListFiles: Exit");
}