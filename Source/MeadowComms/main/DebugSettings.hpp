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
 *                              Bluetooth Settings
 *
 ******************************************************************************/

/**
 *  Log events related to the Bluetooth system.
 */
#define BT_FULL_LOGGING
#undef BT_FULL_LOGGING

#define SPI_DEBUG

#endif /*_MAIN_DEBUGSETTINGS_HPP_ */
