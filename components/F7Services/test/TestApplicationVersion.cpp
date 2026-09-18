/*
 *  TestApplicationVersion.cpp
 *
 *  Provides a dummy version information structure to the tests that require it.
 */

#include "version.h"

/**
 * @brief Global variable holding the system build information.
 *
 * We set every member to 0.
 */
meadow_version_t g_application_version = {};
