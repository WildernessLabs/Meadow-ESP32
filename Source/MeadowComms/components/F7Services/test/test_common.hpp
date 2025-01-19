/**
 * @file test_common.hpp
 * @author Mark Stevens
 * @brief Common methods used by the test classes.
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024
`*/
#ifndef _TEST_COMMON_HPP_
#define _TEST_COMMON_HPP_

#include "sdkconfig.h"

#include "MockMessageDispatcher.hpp"
#include "Encoders.hpp"

extern "C" bool _generateHeapTraceData;

extern MockMessageDispatcher *messageDispatcher;

void WaitForResponse(MockMessageDispatcher *messageDispatcher, uint32_t milliseconds);
Esp32Messaging::IntegerResponse *ExtractIntegerResponse(uint8_t *buffer);
MockMessageDispatcher *SetupMessageDispatcher(void);

#endif // _TEST_COMMON_HPP_