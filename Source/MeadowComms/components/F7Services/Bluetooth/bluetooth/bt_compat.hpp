/**
 * @file bt_compat.hpp
 * @author Chris Tacke
 * 
 * @brief Bluetooth compatibility header.
 * 
 * @copyright Copyright (c) 2018 - 2024
 */
#ifndef _BLUETOOTH_COMPAT_HPP_
#define _BLUETOOTH_COMPAT_HPP_

// Compatibility stuff - let it compile on desktop, etc while keeping ESP flavoring
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif
#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

#endif