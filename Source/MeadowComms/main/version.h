/*
 *  version.h
 *
 *  Hold the version information for the application.
 */

#ifndef __VERSION__H
#define __VERSION__H

#include "../build/config/sdkconfig.h"

#include <stdlib.h>

#define VERSION_MAJOR ###VERSION_MAJOR###
#define VERSION_MINOR ###VERSION_MINOR###
#define VERSION_REVISION ###VERSION_REVISION###
#define VERSION_BUILD ###VERSION_BUILD###
#define VERSION_BUILD_DAY ###BUILD_DAY###
#define VERSION_BUILD_MONTH ###BUILD_MONTH###
#define VERSION_BUILD_MONTH_NAME "###BUILD_MONTH_NAME###"
#define VERSION_BUILD_YEAR ###BUILD_YEAR###
#define VERSION_BUILD_HOUR ###BUILD_HOUR###
#define VERSION_BUILD_MINUTE ###BUILD_MINUTE###
#define VERSION_BUILD_SECOND ###BUILD_SECOND###
#define VERSION_BUILD_HASH "###BUILD_HASH###"
#define VERSION_BUILD_HASH_NUMBER ###BUILD_HASH###
#define VERSION_GIT_REF "###BUILD_GIT_REF###"
#define VERSION_FORMAT_STRING "%d.%d.%d.%d, built %02d %s 20%02d %02d:%02d:%02d UTC (0x%08x/%s)"
#define VERSION_FULL_OS_VERSION "###VERSION_MAJOR###.###VERSION_MINOR###.###VERSION_REVISION###.###VERSION_BUILD### built ###BUILD_TWO_DIGIT_DAY### ###BUILD_MONTH_NAME### 20###BUILD_YEAR### ###BUILD_TWO_DIGIT_HOUR###:###BUILD_TWO_DIGIT_MINUTE###:###BUILD_TWO_DIGIT_SECOND### UTC (###BUILD_HASH###/###BUILD_GIT_REF###)"

struct meadow_version_s
{
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
    uint32_t build;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t year;
    uint32_t hash;
    char *branch_name;
};
typedef struct meadow_version_s meadow_version_t;

extern meadow_version_t g_application_version;

#endif  // __VERSION__H