#ifndef OLED_LUT_H
#define OLED_LUT_H

#include <stdint.h>

#define OLED_LUT_IN_RAM 1   /* 1 = calculée au démarrage (RAM), 0 = précalculée (Flash) */

extern  uint8_t oled_lut[256][4];

#if OLED_LUT_IN_RAM
void oled_lut_init(void);
#endif

#endif
