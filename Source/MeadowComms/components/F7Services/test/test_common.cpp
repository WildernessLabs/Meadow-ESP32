/**
 * @file test_common.cpp
 * @author Mark Stevens
 * @brief Common methods used by the test classes.
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024
 */

#include "test_common.hpp"

/**
 *  Extract the IntegerResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IntegerResponse object.
 *  
 *  @returns
 *      Pointer to a IntegerResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIntegerResponse
Esp32Messaging::IntegerResponse *ExtractIntegerResponse(uint8_t *buffer)
{
    Esp32Messaging::IntegerResponse *integerResponse = (Esp32Messaging::IntegerResponse *) pvPortMalloc(sizeof(Esp32Messaging::IntegerResponse));

    integerResponse->Result = Encoders::ExtractInt32(buffer);
    return(integerResponse);
}

/**
 * @brief Wait for a response to be received. or for a timeout.
 *
 * @note This method allows for a request to take a time to respond.
 *
 * @param milliseconds Number of milliseconds to wait.
 */
void WaitForResponse(MockMessageDispatcher *messageDispatcher, uint32_t milliseconds)
{
    int count = 0;
    while ((messageDispatcher->OutboundMessageQueueLength() == 0) && (count < milliseconds))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        count += 1000;
    }
}
