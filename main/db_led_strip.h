#ifndef _LED_STRIP_
#define _LED_STRIP_
#include<stdint.h>
void led_strip_init();
void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void led_strip_refresh();
void led_strip_set_all_colors(uint8_t r, uint8_t g, uint8_t b);
void led_strip_batt_init();
#endif