/**
 * @file CoreDump.cpp
 * @author Wilderness Labs
 * @brief Core dump handling implementation for the ESP32 application.
 * @version 0.1
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note This file provides the implementation for core dump management and analysis.
 */

#include "CoreDump.hpp"

/**
 * @brief Get the Information about the code dump.
 * 
 * @param pvMessage Pointer to the message requesting the coredump information.
 */
void CoreDump::GetInformation(void * pvMessage)
{
    TRACE_MESSAGE("GetInformation: Enter");

    if (pvMessage == NULL)
    {
        return;
    }

    Esp32Messaging::CoreDumpInformationResponse response = {};
    esp_partition_iterator_t partitions;
    partitions = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (partitions != NULL)
    {
        const esp_partition_t *partition = esp_partition_get(partitions);
        printf("Coredump partition found at offset 0x%" PRIx32 ", size 0x%" PRIx32 "\n",
               partition->address, partition->size);
        if (esp_core_dump_image_check() != ESP_OK)
        {
            response.IsValid = 0;
        }
        else
        {
            response.IsValid = 1;
            response.PartitionSize = partition->size;
            uint8_t buffer[4] = {};
            uint32_t size = 0;
            esp_partition_read(partition, 0, &size, sizeof(size));
            response.CoredumpSize = size;
        }
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
 * @param pvMessage Pointer to the message requesting the core dump fragment.
 */
void CoreDump::GetFragment(void * pvMessage)
{
    TRACE_MESSAGE("GetFragment: Enter");

    if (pvMessage == NULL)
    {
        return;
    }
    message->PayloadLength = 0;
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    if (message->Payload != NULL)
    {
        Encoders::EncodeIntegerResponse(&response, message->Payload);
        message->StatusCode = StatusCodes::CompletedOk;
    }
    else
    {
        message->StatusCode = StatusCodes::EspOutOfMemory;
    }

    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("GetFragment: Exit");
}

/**
 * @brief Erase the core dump.
 * 
 * @param pvMessage Pointer to the message requesting the core dump erasure.
 */
void CoreDump::Erase(void * pvMessage)
{
    TRACE_MESSAGE("Erase: Enter");

    if (pvMessage == NULL)
    {
        return;
    }

    // message->PayloadLength = 0;
    // message->Payload = nullptr;
    // message->StatusCode = StatusCodes::CompletedOk;
    // message->MessageType = MessageTypes::Response;
    // _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Erase: Exit");
}
