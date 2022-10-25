/*
 *  version.h
 *
 *  Hold the version information for the application.
 */

#ifndef __VERSION__H
#define __VERSION__H

#include "../build/config/sdkconfig.h"

#include <stdlib.h>


// #define VERSION_MAJOR               ###VERSION_MAJOR###
// #define VERSION_MINOR               ###VERSION_MINOR###
// #define VERSION_REVISION            ###VERSION_REVISION###
// #define VERSION_BUILD               ###VERSION_BUILD###
// #define VERSION_BUILD_DAY           ###BUILD_DAY###
// #define VERSION_BUILD_MONTH         ###BUILD_MONTH###
// #define VERSION_BUILD_MONTH_NAME    "###BUILD_MONTH_NAME###"
// #define VERSION_BUILD_YEAR          ###BUILD_YEAR###
// #define VERSION_BUILD_HOUR          ###BUILD_HOUR###
// #define VERSION_BUILD_MINUTE        ###BUILD_MINUTE###
// #define VERSION_BUILD_SECOND        ###BUILD_SECOND###
// #define VERSION_BUILD_HASH          "###BUILD_HASH###"
// #define VERSION_BUILD_HASH_NUMBER   ###BUILD_HASH###
// #define VERSION_GIT_REF             "###BUILD_GIT_REF###"


#define VERSION_MAJOR               ###VERSION_MAJOR###
#define VERSION_MINOR               ###VERSION_MINOR###
#define VERSION_REVISION            ###VERSION_REVISION###
#define VERSION_BUILD               ###VERSION_BUILD###
#define VERSION_BUILD_DAY           ###BUILD_DAY###
#define VERSION_BUILD_MONTH         ###BUILD_MONTH###
#define VERSION_BUILD_MONTH_NAME    "###BUILD_MONTH_NAME###"
#define VERSION_BUILD_YEAR          ###BUILD_YEAR###
#define VERSION_BUILD_HOUR          ###BUILD_HOUR###
#define VERSION_BUILD_MINUTE        ###BUILD_MINUTE###
#define VERSION_BUILD_SECOND        ###BUILD_SECOND###
#define VERSION_BUILD_HASH          "###BUILD_HASH###"
#define VERSION_BUILD_HASH_NUMBER   ###BUILD_HASH###
#define VERSION_GIT_REF             "###BUILD_GIT_REF###"
#define VERSION_FORMAT_STRING       "%d.%d.%d.%d, built %02d %s 20%02d %02d:%02d:%02d UTC (0x%08x/%s)"

/**
 * @brief Structure to hold the version information for the system.
 * 
 * Build number is of the format:
 * 
 *      major.minor.revision.build built dd MMM yyy hh:MM:ss UTC (git hash/branch)
 */
struct meadow_version_s
{
    /**
     * @brief Major part of the build number.
     * 
     */
    uint32_t major;

    /**
     * @brief Minor part of the version number.
     */
    uint32_t minor;

    /**
     * @brief Revision part of the version number.
     */
    uint32_t revision;

    /**
     * @brief Build component of the version number.
     */
    uint32_t build;

    /**
     * @brief Day part of the build time.
     */
    uint8_t day;

    /**
     * @brief Month part of the build time.
     */
    uint8_t month;

    /**
     * @brief Year part of the build time.
     */
    uint8_t year;

    /**
     * @brief Hour part of the build time.
     */
    uint8_t hour;

    /**
     * @brief Minute part of the build time.
     */
    uint8_t minute;

    /**
     * @brief Second part of the build time.
     */
    uint8_t second;

    /**
     * @brief Has of the current commit.
     */
    uint32_t hash;

    /**
     * @brief Name of the branch for this build.
     */
    char *branch_name;
};
typedef struct meadow_version_s meadow_version_t;

extern meadow_version_t g_application_version;

#endif  // __VERSION__H