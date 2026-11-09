#pragma once

/**
 * Update the LEDs on the current battery charge (battery_get_percent()).
 * Call periodically, for example from the timer’s clock after battery_update().
 * led_init() must be called in advance.
 */
void battery_leds_update(void);
