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

void WaitForResponse(MockMessageDispatcher *messageDispatcher, uint32_t milliseconds);

#endif // _TEST_COMMON_HPP_