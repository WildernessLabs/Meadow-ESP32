/*
 *  DebugSettings.hpp
 *
 *  The preprocessor variables in this file can be used to switch various logging
 *  output (to the console) on or off.
 *
 *  Generally speaking, defining a variable turns the logging output on, undefining
 *  a variable will turn the output off.
 */

#ifndef _MAIN_DEBUGSETTINGS_HPP_
#define _MAIN_DEBUGSETTINGS_HPP_

/******************************************************************************
 *
 *                          SPI Interface Settings
 *
 ******************************************************************************/

/*
 *  Determine if SPI packets should be output to the logging console.
 */
#define SPI_PACKET_LOGGING 1
#undef SPI_PACKET_LOGGING

/*
 *  Log everything to do with the SPI transport layer.
 */
#define SPI_FULL_LOGGING 1
#undef SPI_FULL_LOGGING

#define SPI_TEST(expression) (expression)

/******************************************************************************
 *
 *                          WiFi Interface Settings
 *
 ******************************************************************************/

/**
 *  Log events related to the WiFi system.
 */
#define WIFI_FULL_LOGGING 1
// #undef WIFI_FULL_LOGGING

#define WIFI_DEBUG_CONNECT 1
#undef WIFI_DEBUG_CONNECT

#define WIFI_DEBUG_POLL 1
#undef WIFI_DEBUG_POLL

#define WIFI_DEBUG_RECVFROM_DATA 1
#undef WIFI_DEBUG_RECVFROM_DATA

#define WIFI_DEBUG_SETSOCKOPT 1
#undef WIFI_DEBUG_SETSOCKOPT

#define WIFI_DEBUG_SEND_DATA 1
#undef WIFI_DEBUG_SEND_DATA

#define WIFI_DEBUG_SENDTO_DATA 1
#undef WIFI_DEBUG_SENDTO_DATA

#define WIFI_DEBUG_SELECT 2
#undef WIFI_DEBUG_SELECT

#define WIFI_DEBUG_SOCKET 2
#undef WIFI_DEBUG_SOCKET

/******************************************************************************
 *
 *                              System Settings
 *
 ******************************************************************************/

/**
 *  Log events related to the system.
 */
#define SYSTEM_FULL_LOGGING
#undef SYSTEM_FULL_LOGGING

/**
 * 
 */
extern char *enterMethodString;
extern char *exitMethodString;

#endif /*_MAIN_DEBUGSETTINGS_HPP_ */
