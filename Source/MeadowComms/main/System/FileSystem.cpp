/*
 *  FileSystem.cpp
 *
 *  Implementation of the methods to provide the file system functionality for the STM32.
 */

#include "../build/config/sdkconfig.h"

#include "esp_littlefs.h"

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

StatusCodes::StatusCodes FileSystem::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    //
    //  Use settings defined above to initialize and mount LittleFS filesystem.
    //
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