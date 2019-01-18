/*
 * Encoders.hpp
 *
 *  Created on: 18 Jan 2019
 *      Author: markstevens
 */

#ifndef MAIN_ENCODERS_HPP_
#define MAIN_ENCODERS_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>

#define CRC32_SEED                  (0xffffffff)

class Encoders
{
public:
	static uint16_t ExtractUInt16(uint8_t *);
	static void EncodeUInt16(uint16_t, uint8_t *);
	static uint32_t ExtractUInt32(uint8_t *);
	static void EncodeUInt32(uint32_t, uint8_t *);
	static uint8_t CRC8(const uint8_t *, uint16_t);
	static uint32_t CRC32(const uint8_t *, uint16_t);
	static uint32_t ProgressiveCRC32(uint32_t, uint8_t);
};


#endif /* MAIN_ENCODERS_HPP_ */
