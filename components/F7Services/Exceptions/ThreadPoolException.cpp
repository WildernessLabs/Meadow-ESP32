/**
 * @file ThreadPoolException.cpp
 * @author Mark Stevens
 * @brief Implementation of the ThreadPoolException class.
 * @date 2021-01-14
 * 
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "ThreadPoolException.hpp"

/**
 *  @brief Default constructor for the ThreadPoolException class.
 */
ThreadPoolException::ThreadPoolException() : std::runtime_error("ThreadPoolException")
{
}

/**
 *  @brief Create an instance of the ThreadPoolException class with the specified message.
 *
 *  @param message
 *      Message to be used in the construction of the instance of the ThreadPoolException class.
 */
ThreadPoolException::ThreadPoolException(const std::string &message) : std::runtime_error(message)
{
}
