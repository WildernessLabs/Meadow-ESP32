/**
 * @file ThreadPoolException.hpp
 * @author Mark Stevens
 * @brief Provide an exception for thread pool related errors.
 * @date 2021-01-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <stdexcept>
#include <string>

/**
 *  @brief Exceptions thrown by the Thread and the ThreadPool classes.
 */
class ThreadPoolException : public std::runtime_error
{
public:
    /**
     *  @brief Default constructor.
     */
    ThreadPoolException();

    /**
     *  @brief Constructor using a given text message.
     */
    explicit ThreadPoolException(const std::string &message);
};
