/**
 * @file FileSystem.cpp
 * @author Mark Stevens
 * @brief Implementation of the FileSystem class for working with files on the ESP32.
 * @date 2023-01-08
 *
 * @copyright Copyright (c) 2023 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "IMessageDispatcher.hpp"
#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "FileSystem.hpp"
#include "Mapping.hpp"
#include "Utils.hpp"
#include "Logging.hpp"

#if defined(FILE_SYSTEM_DEBUG)
    #pragma message "FileSystem.cpp: Debugging enabled"
#endif

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
const char *FileSystem::COMPONENT_NAME = "FileSystem";

/**
 *  @brief Name of the partition in the partitions.csv file.
 */
const char *FileSystem::PARTITION_PATH_NAME = "/littlefs";

/**
 *  @brief Label used for the partition when formatting.
 */
const char *FileSystem::PARTITION_LABEL = "littlefs";

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
    _config = {};
    _config.base_path = PARTITION_PATH_NAME;
    _config.partition_label = PARTITION_LABEL;
    _config.format_if_mount_failed = true;
    _config.dont_mount = false;
    //
    //  The following options are not used in LittleFS version 1.5.5.
    //  They are left here for future reference if we uplift LittleFS to a
    //  more recent version.
    //
    // _config.read_only = false;
    // _config.partition = NULL;
    // _config.grow_on_mount = true;
}

/**
 * @brief Construct a new File System object
 *
 * @param messageDispatcher
 *      Message dispatcher object used to queue messages for the STM32.
 */
FileSystem::FileSystem(IMessageDispatcher *messageDispatcher) : FileSystem()
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
 * @brief Show the list of files on the file system.
 */
void FileSystem::ShowFiles()
{
    TRACE_MESSAGE("Files on file system:");
    DIR *dp = opendir(FileSystem::PARTITION_PATH_NAME);
    if (dp != NULL)
    {
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += ep->d_name;
            struct stat fileInfo;
            if (stat(s.c_str(), &fileInfo) == 0)
            {
                TRACE_MESSAGE("    %s, %" PRIu32, ep->d_name, fileInfo.st_size);
            }
            else
            {
                TRACE_MESSAGE("    %s, stat failed: %s", ep->d_name, strerror(errno));
            }
        }
        closedir(dp);
    }
    else
    {
        TRACE_MESSAGE("Failed to open directory %s", FileSystem::PARTITION_PATH_NAME);
    }
    TRACE_MESSAGE("End of file list.");
}

/**
 * @brief Mount the file system.
 *
 * @return true If the file system is mounted OK.
 * @return false If the file is not mounted.
 */
bool FileSystem::IsMounted()
{
    return (esp_littlefs_mounted(_config.partition_label));
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
        result = false;
    }

    return (result);
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

    return (result);
}

/**
 * @brief Setup the file system.
 *
 * @return StatusCodes::CompletedOk on success, StatusCodes::Failure otherwise.
 */
StatusCodes::StatusCodes FileSystem::Setup()
{
    TRACE_MESSAGE("Setup: Enter");
    ESP_LOGI(COMPONENT_NAME, "Setting up file system");

    StatusCodes::StatusCodes result;
    if (Mount())
    {
#if defined(FILE_SYSTEM_DEBUG)
        ShowFiles();
#endif

        result = StatusCodes::CompletedOk;
    }
    else
    {
        result = StatusCodes::Failure;
    }

    TRACE_MESSAGE("Setup: Exit");

    return (result);
}

/**
 * @brief Format the file system.
 *
 * @param message
 *      Pointer to the originating message object.
 */
void FileSystem::Format(Message * const message)
{
    TRACE_MESSAGE("Format: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    message->DeletePayload(); // There should be no payload.

    esp_err_t ret = esp_littlefs_format(PARTITION_LABEL);
    if (ret == ESP_OK)
    {
        size_t total = 0, used = 0;
        ret = esp_littlefs_info(PARTITION_LABEL, &total, &used);
        if (ret == ESP_OK)
        {
            TRACE_MESSAGE("Partition size: total: %" PRIu32 ", used: %" PRIu32, total, used);
            result = StatusCodes::CompletedOk;
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Format: Exit");
}

/**
 * @brief Read a file from the file system.
 *
 * @param message
 *      Pointer to the originating message object.
 */
void FileSystem::ReadFile(Message * const message)
{
    TRACE_MESSAGE("ReadFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileNameAndContents *fileData = Encoders::ExtractFileNameAndContents(message->Payload);
    message->DeletePayload();

    if (fileData)
    {
        if (fileData->Name != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += fileData->Name;
            struct stat fileInfo;
            if (stat(s.c_str(), &fileInfo) == 0)
            {
                fileData->ContentsLength = 0;
                fileData->Contents = static_cast<uint8_t *>(pvPortMalloc(fileInfo.st_size));
                if (fileData->Contents != NULL)
                {
                    TRACE_MESSAGE("Reading %" PRIu32 " bytes from file %s", fileInfo.st_size, fileData->Name);
                    FILE *file = fopen(s.c_str(), "rb");
                    if (file != NULL)
                    {
                        int amountRead = fread(static_cast<void *>(fileData->Contents), 1, fileInfo.st_size, file);
                        if (amountRead > 0)
                        {
                            fileData->ContentsLength = amountRead;
                            TRACE_MESSAGE("Read %" PRIi32 " bytes", amountRead);
                            TRACE_HEX_BUFFER(fileData->Contents, amountRead);
                            fileData->ContentsLength = amountRead;
                        }
                        fclose(file);
                        message->PayloadLength = Encoders::EncodedFileNameAndContentsBufferSize(fileData);
                        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                        if (message->Payload == NULL)
                        {
                            message->PayloadLength = 0;
                            result = StatusCodes::EspOutOfMemory;
                        }
                        else
                        {
                            Encoders::EncodeFileNameAndContents(fileData, message->Payload);
                            result = StatusCodes::CompletedOk;
                        }
                    }
                    if (result != StatusCodes::CompletedOk)
                    {
                        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData->Contents));
                        fileData->ContentsLength = 0;
                    }
                }
                else
                {
                    fileData->ContentsLength = 0;
                    result = StatusCodes::EspOutOfMemory;
                }
            }
            else
            {
                result = StatusCodes::FileNotFound;
            }
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData->Name));
        if (fileData->Contents != NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData->Contents));
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData));
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("ReadFile: Exit");
}

/**
 * @brief Write a file to the file system.
 *
 * @param message
 *      Pointer to the originating message object.
 */
void FileSystem::WriteFile(Message * const message)
{
    TRACE_MESSAGE("WriteFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileNameAndContents *fileData = Encoders::ExtractFileNameAndContents(message->Payload);
    message->DeletePayload();

    if (fileData)
    {
        if (fileData->Contents)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += fileData->Name;
            TRACE_MESSAGE("Writing %lu bytes into file %s", fileData->ContentsLength, s.c_str());
            TRACE_HEX_BUFFER(fileData->Contents, fileData->ContentsLength);
            FILE *file = fopen(s.c_str(), "w");
            if (file != NULL)
            {
                int amount = fwrite((void *) fileData->Contents, 1, fileData->ContentsLength, file);
                TRACE_MESSAGE("Written %" PRIi32 " bytes", amount);
                if (amount == fileData->ContentsLength)
                {
                    result = StatusCodes::CompletedOk;
                }
                fclose(file);
            }

            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData->Contents));
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileData));
    }
    else
    {
        result = StatusCodes::EspOutOfMemory;
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("WriteFile: Exit");
}

/**
 * @brief Delete a file on the file system.
 *
 * @param message
 *      Pointer to the originating message object.
 */
void FileSystem::DeleteFile(Message * const message)
{
    TRACE_MESSAGE("DeleteFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileDetails *details = Encoders::ExtractFileDetails(message->Payload);
    message->DeletePayload();

    if (details)
    {
        if (details->Name != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += details->Name;

            if (remove(s.c_str()) == 0)
            {
                result = StatusCodes::CompletedOk;
            }
            else
            {
                if (errno == ENOENT)
                {
                    result = StatusCodes::FileNotFound;
                }
            }
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&details->Name));
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&details));
    }
    else
    {
        result = StatusCodes::EspOutOfMemory;
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("DeleteFile: Exit");
}

/**
 * @brief List the files on the file system.
 *
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::ListFiles(Message *message)
{
    TRACE_MESSAGE("ListFiles: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    message->DeletePayload();

    int count = 0;
    DIR *dp = opendir(FileSystem::PARTITION_PATH_NAME);
    if (dp != NULL)
    {
        TRACE_MESSAGE("Directory %s opened.", FileSystem::PARTITION_PATH_NAME);
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL)
        {
            count++;
        }
        rewinddir(dp);
        Esp32Messaging::FileNameList fileNameList;
        memset(&fileNameList, 0, sizeof(fileNameList));
        uint32_t bufferLength = 0;
        TRACE_MESSAGE("Found %" PRIi32 " files.", count);
        if (count > 0)
        {
            while (((ep = readdir(dp)) != NULL) && (result != StatusCodes::EspOutOfMemory))
            {
                Esp32Messaging::FileDetails details;
                memset(&details, 0, sizeof(details));
                //
                //  details.Name borrows the storage owned by the DIR object.  It is only valid until the next readdir or closedir call.
                //
                details.Name = ep->d_name;
                std::string s = PARTITION_PATH_NAME;
                s += "/";
                s += ep->d_name;
                struct stat fileInfo;
                if (stat(s.c_str(), &fileInfo) == 0)
                {
                    details.Length = fileInfo.st_size;
                }
                else
                {
                    details.Length = 0;
                }
                TRACE_MESSAGE("    File: %s, length: %" PRIu16, details.Name, details.Length);
                bufferLength += Encoders::EncodedFileDetailsBufferSize(&details);
                uint8_t *newBuffer = static_cast<uint8_t *>(Utils::Realloc(fileNameList.FileDetails, fileNameList.FileDetailsLength, bufferLength));
                if (newBuffer != NULL)
                {
                    fileNameList.FileDetails = newBuffer;
                    Encoders::EncodeFileDetails(&details, fileNameList.FileDetails + fileNameList.FileDetailsLength);
                    fileNameList.FileDetailsLength = bufferLength;
                }
                else
                {
                    result = StatusCodes::EspOutOfMemory;
                }
            }
        }
        closedir(dp);
        if (result != StatusCodes::EspOutOfMemory)
        {
            fileNameList.NumberOfFiles = count;
            message->PayloadLength = Encoders::EncodedFileNameListBufferSize(&fileNameList);
            message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
            if (message->Payload == NULL)
            {
                message->PayloadLength = 0;
                result = StatusCodes::EspOutOfMemory;
            }
            else
            {
                Encoders::EncodeFileNameList(&fileNameList, message->Payload);
                result = StatusCodes::CompletedOk;
            }
        }
        if (result == StatusCodes::EspOutOfMemory)
        {
            message->PayloadLength = 0;
            message->Payload = nullptr;
        }
        if (fileNameList.FileDetails != NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileNameList.FileDetails));
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("ListFiles: Exit");
}