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
//#undef SPI_PACKET_LOGGING

/*
 *  Log everything to do with the SPI transport layer.
 */
#define SPI_FULL_LOGGING 1
//#undef SPI_FULL_LOGGING

#define SPI_TEST(expression) (expression)

/******************************************************************************
 *
 *                          WiFi Interface Settings
 *
 ******************************************************************************/

/*
 *  Log everything to do with the WiFi tasks.
 */
#define WIFI_FULL_LOGGING 1
//#undef WIFI_FULL_LOGGING

#endif /*_MAIN_DEBUGSETTINGS_HPP_ */
