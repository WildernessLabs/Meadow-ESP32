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

    if (vpMessage == nullptr)
    {
        return;
    }
    Message * const message = static_cast<Message *>(vpMessage);

    Esp32Messaging::CoreDumpInformationResponse response = {};
    esp_partition_iterator_t partitions;
    partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (partitions != nullptr)
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

    //
    //  This request (message) has no payload so there is nothing to delete.
    //
    message->PayloadLength = Encoders::EncodedCoreDumpInformationBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    if (message->Payload != nullptr)
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
 *  The fragment returned is the smallest of the size asked for, what is left of the dump
 *  and MAXIMUM_FRAGMENT_SIZE.  A request for nothing, or one starting at or past the end
 *  of the dump, is refused with InvalidRequest.
 *
 *  The image checksum is worked out only for the request at offset zero, as it reads the
 *  whole partition and a retrieval is made of many requests.  A dump whose checksum does
 *  not match is refused with CrcError.
 *
 * @param vpMessage Pointer to the message requesting the core dump fragment.
 */
void CoreDump::GetFragment(void * const vpMessage)
{
    TRACE_MESSAGE("GetFragment: Enter");

    if (vpMessage == nullptr)
    {
        return;
    }

    Message * const message = static_cast<Message *>(vpMessage);
    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::CoreDumpFragmentResponse response = {};
    Esp32Messaging::CoreDumpFragmentRequest *request = Encoders::ExtractCoreDumpFragmentRequest(message->Payload);
    if (request == nullptr)
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
        partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
        if (partitions != nullptr)
        {
            const esp_partition_t *partition = esp_partition_get(partitions);
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
            //
            //  Verify the checksum for the first fragment only as it can take 900-1000 milliseconds.
            //
            bool imageIsValid = true;

            if (request->Offset == 0)
            {
                imageIsValid = (esp_core_dump_image_check() == ESP_OK);
            }

            if (!imageIsValid)
            {
                result = StatusCodes::CrcError;
            }
            else
            {
                uint32_t size = 0;
                if (esp_partition_read(partition, 0, &size, sizeof(size)) == ESP_OK)
                {
                    const bool sizeIsSensible = ((size > 0) && (size <= partition->size));

                    if (sizeIsSensible)
                    {
                        const bool requestIsValid = ((request->Size > 0) && (request->Offset < size));

                        if (!requestIsValid)
                        {
                            result = StatusCodes::InvalidRequest;
                        }
                        else
                        {
                            uint32_t fragmentSize = size - request->Offset;
                            if (fragmentSize > request->Size)
                            {
                                fragmentSize = request->Size;
                            }
                            if (fragmentSize > MAXIMUM_FRAGMENT_SIZE)
                            {
                                fragmentSize = MAXIMUM_FRAGMENT_SIZE;
                            }

                            uint8_t *data = static_cast<uint8_t *>(pvPortMalloc(fragmentSize));
                            if (data == nullptr)
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
                                    Utils::DeleteAndNullifyPointer(data);
                                }
                            }
                        }
                    }
                }
            }
#endif
            esp_partition_iterator_release(partitions);
        }
    }

    Utils::DeleteAndNullifyPointer(request);
    message->DeletePayload();

    if (result == StatusCodes::CompletedOk)
    {
        message->PayloadLength = Encoders::EncodedCoreDumpFragmentResponseBufferSize(&response);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload != nullptr)
        {
            Encoders::EncodeCoreDumpFragmentResponse(&response, message->Payload);
            message->StatusCode = StatusCodes::CompletedOk;
        }
        else
        {
            message->PayloadLength = 0;
            message->StatusCode = StatusCodes::EspOutOfMemory;
        }
    }
    else
    {
        message->StatusCode = result;
    }
    Utils::DeleteAndNullifyPointer(response.Data);

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

    if (vpMessage == nullptr)
    {
        return;
    }

    Message * const message = static_cast<Message *>(vpMessage);
    StatusCodes::StatusCodes result = StatusCodes::Failure;

    esp_partition_iterator_t partitions;
    partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (partitions != nullptr)
    {
        const esp_partition_t *partition = esp_partition_get(partitions);
        if (esp_partition_erase_range(partition, 0, partition->size) == ESP_OK)
        {
            result = StatusCodes::CompletedOk;
        }
        esp_partition_iterator_release(partitions);
    }

    message->DeletePayload();
    message->StatusCode = result;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Erase: Exit");
}
