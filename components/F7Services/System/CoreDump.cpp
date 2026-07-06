/**
 * @file CoreDump.cpp
 * @author Mark Stevens
 * @brief Core dump handling implementation for the ESP32 application.
 * @date 2025-08-24
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note This file provides the implementation for core dump management and analysis.
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include "esp_err.h"
#include "esp_partition.h"

#include "Logging.hpp"
#include "CoreDump.hpp"
#include "Utils.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"

/**
 * @brief Check the validity of the core dump image.
 *
 * @note This function is defined in the ESP-IDF framework and is only available
 *       when CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH is set.
 *
 * @return esp_err_t ESP_OK if the core dump image is valid, otherwise an error code.
 */
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
extern "C" esp_err_t esp_core_dump_image_check();
#endif

#if defined(CORE_DUMP_DEBUG)
    #pragma message "CoreDump.cpp: Debug logging enabled"
#endif

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief CoreDump component name.
 */
const char *CoreDump::COMPONENT_NAME = "CoreDump";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new Core Dump:: Core Dump object
 *
 * @param messageDispatcher Message dispatcher object used for communication with the STM32.
 */
CoreDump::CoreDump(IMessageDispatcher *messageDispatcher) : _messageDispatcher(messageDispatcher)
{
    TRACE_MESSAGE("CoreDump: Constructor with dispatcher");
}

/**
 * @brief Destroy the Core Dump object.
 */
CoreDump::~CoreDump()
{
    TRACE_MESSAGE("CoreDump: Destructor");
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
 * @brief Get the Information about the code dump.
 *
 * @param vpMessage Pointer to the message requesting the coredump information.
 */
void CoreDump::GetInformation(void * const vpMessage)
{
    TRACE_MESSAGE("GetInformation: Enter");

    if (vpMessage == NULL)
    {
        return;
    }
    Message * const message = static_cast<Message *>(vpMessage);

    Esp32Messaging::CoreDumpInformationResponse response = {};
    esp_partition_iterator_t partitions;
    partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (partitions != NULL)
    {
        const esp_partition_t *partition = esp_partition_get(partitions);
        response.PartitionSize = partition->size;
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
        if (esp_core_dump_image_check() != ESP_OK)
        {
            response.IsValid = 0;
        }
        else
        {
            response.IsValid = 1;
            uint32_t size = 0;
            esp_partition_read(partition, 0, &size, sizeof(size));
            response.CoreDumpSize = size;
        }
#else
        response.IsValid = 0;
#endif
        esp_partition_iterator_release(partitions);
    }
    else
    {
        response.IsValid = 0;
    }

    message->PayloadLength = Encoders::EncodedCoreDumpInformationBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    if (message->Payload != NULL)
    {
        Encoders::EncodeCoreDumpInformation(&response, message->Payload);
        message->StatusCode = StatusCodes::CompletedOk;
    }
    else
    {
        message->StatusCode = StatusCodes::EspOutOfMemory;
    }

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("GetInformation: Exit");
}

/**
 * @brief Get part (maybe all) of the core dump.
 *
 * @param vpMessage Pointer to the message requesting the core dump fragment.
 */
void CoreDump::GetFragment(void * const vpMessage)
{
    TRACE_MESSAGE("GetFragment: Enter");

    if (vpMessage == NULL)
    {
        return;
    }

    Message * const message = static_cast<Message *>(vpMessage);
    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::CoreDumpFragmentResponse response = {};
    Esp32Messaging::CoreDumpFragmentRequest *request = Encoders::ExtractCoreDumpFragmentRequest(message->Payload);
    if (request == NULL)
    {
        if (message->Payload)
        {
            result = StatusCodes::EspOutOfMemory;
        }
        else
        {
            result = StatusCodes::InvalidRequest;
        }
    }
    else
    {
        esp_partition_iterator_t partitions;
        partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
        if (partitions != NULL)
        {
            const esp_partition_t *partition = esp_partition_get(partitions);
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
            if (esp_core_dump_image_check() == ESP_OK)
            {
                uint32_t size = 0;
                if (esp_partition_read(partition, 0, &size, sizeof(size)) == ESP_OK)
                {
                    uint32_t fragmentSize = size;
                    if (request->Offset < size)
                    {
                        fragmentSize = size - request->Offset;
                        if (fragmentSize > request->Size)
                        {
                            fragmentSize = request->Size;
                        }
                    }
                    uint8_t *data = static_cast<uint8_t *>(pvPortMalloc(fragmentSize));
                    if (data == NULL)
                    {
                        result = StatusCodes::EspOutOfMemory;
                    }
                    else
                    {
                        if (esp_partition_read(partition, request->Offset, data, fragmentSize) == ESP_OK)
                        {
                            response.Data = data;
                            response.Size = fragmentSize;
                            result = StatusCodes::CompletedOk;
                        }
                        else
                        {
                            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&data));
                        }
                    }
                }
            }
#endif
            esp_partition_iterator_release(partitions);
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        message->PayloadLength = Encoders::EncodedCoreDumpFragmentResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != NULL)
        {
            Encoders::EncodeCoreDumpFragmentResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->Payload = nullptr;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        message->PayloadLength = 0;
        message->Payload = nullptr;
        message->StatusCode = result;
    }

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("GetFragment: Exit");
}

/**
 * @brief Erase the core dump.
 *
 * @param vpMessage Pointer to the message requesting the core dump erasure.
 */
void CoreDump::Erase(void * const vpMessage)
{
    TRACE_MESSAGE("Erase: Enter");

    if (vpMessage == NULL)
    {
        return;
    }

    Message * const message = static_cast<Message *>(vpMessage);
    StatusCodes::StatusCodes result = StatusCodes::Failure;

    esp_partition_iterator_t partitions;
    partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (partitions != NULL)
    {
        const esp_partition_t *partition = esp_partition_get(partitions);
        if (esp_partition_erase_range(partition, 0, partition->size) == ESP_OK)
        {
            result = StatusCodes::CompletedOk;
        }
        esp_partition_iterator_release(partitions);
    }

    message->PayloadLength = 0;
    message->Payload = nullptr;
    message->StatusCode = result;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Erase: Exit");
}
