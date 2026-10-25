#ifndef HAL_CALLBACK_PROBE_H
#define HAL_CALLBACK_PROBE_H
#include <stdint.h>
#include "stm32f4xx_hal.h"
extern volatile uint32_t callback_probe_hits;
HAL_StatusTypeDef callback_probe_start(void);
#endif
