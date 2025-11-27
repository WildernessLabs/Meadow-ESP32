/**
 * @file NotImplementedException.hpp
 * @author Mark Stevens
 * @brief Provide an exception for when a feature or method is not implemented.
 * @date 2019-09-17
 * 
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include <stdexcept>

class NotImplementedException : public std::runtime_error
{
public:
    NotImplementedException() : std::runtime_error("NotImplementedException")
    {
    }
};
