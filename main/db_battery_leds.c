#include "db_battery_leds.h"
#if CONFIG_IDF_TARGET_ESP32C3
#include <stdbool.h>
#include <stdint.h>
#include "db_battery_indicator.h"
#include "db_led_strip.h"   

#define BAT_LED_COUNT  4
#define BAT_LED_STEP   (100.0f / BAT_LED_COUNT)   /* 25% per LED  */
#define BAT_LED_HYST   3.0f                       /* Hysteresis, % (to avoid flickering at borders) */


static const uint8_t s_colors[BAT_LED_COUNT][3] = {
    {20, 0,  0},    /* 1: red */
    {20, 10, 0},    /* 2: orange */
    {0,  20, 0},    /* 3: green */
    {0,  20, 0},    /* 4: green */
};


static bool s_lit[BAT_LED_COUNT];
static int  s_prev_level = -1;    /* -1 = nothing yet */

void battery_leds_update(void)
{
    float percent = battery_get_percent();
        /* The LED i burns when the charge is above i * 25%. The first always burns. */
    int level = 0;
    for (int i = 0; i < BAT_LED_COUNT; i++) {
        if (i == 0) {
            s_lit[i] = true;
        } else {
            float threshold = i * BAT_LED_STEP;
            if (s_lit[i]) {
                if (percent < threshold - BAT_LED_HYST) {
                    s_lit[i] = false;
                }
            } else if (percent > threshold + BAT_LED_HYST) {
                s_lit[i] = true;
            }
        }
        if (s_lit[i]) {
            level++;
        }
    }

    /* Update LEDs only when level change */
    if (level == s_prev_level) {
        return;
    }
    s_prev_level = level;
    uint8_t s_led_map[BAT_LED_COUNT] = {3,2,1,0};
    const uint8_t *c = s_colors[level-1];
    for (int i = 0; i < BAT_LED_COUNT; i++) {
        if (s_lit[i]) {
            led_strip_set_pixel(s_led_map[i], c[0], c[1], c[2]);
        } else {
            led_strip_set_pixel(s_led_map[i], 0, 0, 0);
        }
    }
    led_strip_refresh();
}

#endif