/**
 * @file Logging.hpp
 * @author Mark Stevens
 *
 * @brief Dummy Logging definitions used in the tests to allow the tests to compile.
 *
 * @copyright Copyright (c) 2025
 */
#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

#define TRACE_MESSAGE trace_message

extern "C" void trace_message(const char *format, ...);

#endif // __LOGGING_HPP__