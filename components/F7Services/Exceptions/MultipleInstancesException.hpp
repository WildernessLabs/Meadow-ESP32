/**
 * @file MultipleInstancesException.hpp
 * @author Mark Stevens
 * @brief Provide an exception for when the Singleton pattern is broken.
 * @date 2019-02-13
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include <stdexcept>

class MultipleInstancesException : public std::runtime_error
{
public:
    MultipleInstancesException() : std::runtime_error("MultipleInstancesException")
    {
    }
};
