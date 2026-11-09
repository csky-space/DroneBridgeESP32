#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ADC initialization (GPIO1, divisor 10k/1k).
 * After initialization, the number of cells (2S..6S) is automatically determined
 * Based on current battery voltage. The battery must be connected.
 */
esp_err_t battery_init(void);

/**
 * Set the number of cells manually: 2.. 6.
 * 0 = auto-redefine (by current voltage).
 */
esp_err_t battery_set_cell_count(uint8_t cell_count);

/** Current cell count (2.6). */
uint8_t battery_get_cell_count(void);

/** Full battery voltage, mV. */
esp_err_t battery_read_mv(uint32_t *battery_mv);

/**
 * One dimension with smoothing: updates values for battery_get_mv() and
 * battery_get_percent(). Call periodically (recommended once per second)
 * from a single location, such as a clockwork timer
 */
void battery_update(void);

/** Last filtered voltage, MWe (without ADC). 0, no measurements yet. */
uint32_t battery_get_mv(void);

/** Last filtered charge, 0.0 .. 100.0% (without ADC). */
float battery_get_percent(void);

/** Battery charge, 0.0 .. 100.0%. */
esp_err_t battery_read_percent(float *percent);

/** Convert the battery voltage (MW) to percentage for a given number of cells. */
float battery_mv_to_percent(uint32_t battery_mv, uint8_t cell_count);

#ifdef __cplusplus
}
#endif
