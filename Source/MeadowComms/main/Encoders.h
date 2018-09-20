#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "stdlib.h"

extern uint32_t ExtractUInt16(uint8_t *);
extern void EncodeUInt16(uint16_t, uint8_t *);
extern uint32_t ExtractUInt32(uint8_t *);
extern void EncodeUInt32(uint32_t, uint8_t *);