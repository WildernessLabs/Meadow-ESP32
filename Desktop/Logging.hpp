/**
 * @file Logging.hpp
 * @author Mark Stevens
 *
 * @brief Dummy Logging definitions used in the tests to allow the tests to compile.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#define TRACE_MESSAGE trace_message

extern "C" void trace_message(const char *format, ...);
