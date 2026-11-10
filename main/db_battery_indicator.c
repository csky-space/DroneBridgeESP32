#include"db_battery_indicator.h"
#include <stddef.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define BAT_ADC_UNIT      ADC_UNIT_1
#define BAT_ADC_CHANNEL   ADC_CHANNEL_1        /* GPIO1 */
#define BAT_ADC_ATTEN     ADC_ATTEN_DB_12      /* input до ~2.45 V  on the pin*/
#define BAT_R_TOP         10000                /* R, Vin*/
#define BAT_R_BOTTOM      1000                 /* R, GND*/
#define BAT_SAMPLES       16                   /* number of measurements to average */

#define BAT_MIN_CELLS     2
#define BAT_MAX_CELLS     6
#define BAT_CELL_MIN_MV   3500                 /* min mV */
#define BAT_CELL_MAX_MV   4250                 /* max mV*/
#define BAT_FILTER_ALPHA  0.2f     

static float             s_filtered_mv = -1.0f;   /* < 0 = no measurements yet */
static volatile uint32_t s_last_mv     = 0;
static const char *TAG = "battery";
static adc_oneshot_unit_handle_t s_adc  = NULL;
static adc_cali_handle_t         s_cali = NULL;
static uint8_t                   s_cells = BAT_MIN_CELLS;

/* Shear of one cell LiPo (to rest): voltage, mV -> charge, %.
   Order: from higher to lower voltage */
typedef struct {
    float mv;
    float percent;
} curve_point_t;

static const curve_point_t s_curve[] = {
    {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80},
    {3980, 75},  {3950, 70}, {3910, 65}, {3870, 60}, {3850, 55},
    {3840, 50},  {3820, 45}, {3800, 40}, {3790, 35}, {3770, 30},
    {3750, 25},  {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5},
    {3270, 0},
};
#define CURVE_LEN (sizeof(s_curve) / sizeof(s_curve[0]))

float battery_mv_to_percent(uint32_t battery_mv, uint8_t cell_count)
{
    if (cell_count == 0) {
        return 0.0f;
    }

    float cell_mv = (float)battery_mv / cell_count;

    if (cell_mv >= s_curve[0].mv) {
        return 100.0f;
    }
    if (cell_mv <= s_curve[CURVE_LEN - 1].mv) {
        return 0.0f;
    }

    
    for (size_t i = 0; i < CURVE_LEN - 1; i++) {
        const curve_point_t *hi = &s_curve[i];
        const curve_point_t *lo = &s_curve[i + 1];
        if (cell_mv <= hi->mv && cell_mv >= lo->mv) {
            float k = (cell_mv - lo->mv) / (hi->mv - lo->mv);
            return lo->percent + k * (hi->percent - lo->percent);
        }
    }
    return 0.0f;
}
static int32_t measure_mv(void)
{
    if (s_adc == NULL) {
        return -(int32_t)ESP_ERR_INVALID_STATE;
    }

    uint32_t sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_adc, BAT_ADC_CHANNEL, &raw);
        if (err != ESP_OK) {
            return -(int32_t)err;
        }
        sum += (uint32_t)raw;
    }

    
    int pin_mv = 0;
    esp_err_t err = adc_cali_raw_to_voltage(s_cali, (int)(sum / BAT_SAMPLES), &pin_mv);
    if (err != ESP_OK) {
        return -(int32_t)err;
    }

    /* Vbat = Vpin * (R_top + R_bottom) / R_bottom */
    return (int32_t)((uint32_t)pin_mv * (BAT_R_TOP + BAT_R_BOTTOM) / BAT_R_BOTTOM);
}

esp_err_t battery_read_mv(uint32_t *battery_mv)
{
    if (battery_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int32_t mv = measure_mv();
    if (mv < 0) {
        return (esp_err_t)(-mv);
    }
    *battery_mv = (uint32_t)mv;
    return ESP_OK;
}

/* Auto-number of cells: the smallest N (2..6) at which per cell
   not more than 4.25 V. For battery within 3.5.. 4.25 V per cell
   The result is unambiguous. You need to measure without any load.  */
static esp_err_t detect_cells(void)
{
    uint32_t mv = 0;
    esp_err_t err = battery_read_mv(&mv);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t cells = BAT_MAX_CELLS;
    for (uint8_t n = BAT_MIN_CELLS; n < BAT_MAX_CELLS; n++) {
        if (mv <= (uint32_t)n * BAT_CELL_MAX_MV) {
            cells = n;
            break;
        }
    }
    s_cells = cells;

    /* Cell voltage outside 3.5.. 4.25 V: battery discharged, not connected
       or has non-standard voltage - the result may be incorrect  */
    uint32_t cell_mv = mv / cells;
    if (cell_mv < BAT_CELL_MIN_MV || cell_mv > BAT_CELL_MAX_MV) {
        ESP_LOGW(TAG, "Battery %lu mV is out of expected range, assuming %uS. Set cell count manually",
                 (unsigned long)mv, (unsigned)cells);
    }
    return ESP_OK;
}

esp_err_t battery_set_cell_count(uint8_t cell_count)
{
    if (cell_count == 0) {
        return detect_cells();
    }
    if (cell_count < BAT_MIN_CELLS || cell_count > BAT_MAX_CELLS) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cells = cell_count;
    return ESP_OK;
}

uint8_t battery_get_cell_count(void)
{
    return s_cells;
}

esp_err_t battery_init(void)
{
    if (s_adc != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BAT_ADC_UNIT,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        s_adc = NULL;
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc, BAT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
        return err;
    }

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = BAT_ADC_UNIT,
        .chan     = BAT_ADC_CHANNEL,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
        s_cali = NULL;
        return err;
    }

   
    err = detect_cells();
    if (err == ESP_OK) {
        battery_update();
    }
    return err;
}

esp_err_t battery_read_percent(float *percent)
{
    if (percent == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t mv = 0;
    esp_err_t err = battery_read_mv(&mv);
    if (err != ESP_OK) {
        return err;
    }

    *percent = battery_mv_to_percent(mv, s_cells);
    return ESP_OK;
}



void battery_update(void)
{
    int32_t r = measure_mv();
    if (r < 0) {
        return;
    }
    uint32_t mv = (uint32_t)r;

    if (s_filtered_mv < 0.0f) {
        s_filtered_mv = (float)mv;
    } else {
        s_filtered_mv += BAT_FILTER_ALPHA * ((float)mv - s_filtered_mv);
    }
    s_last_mv = (uint32_t)s_filtered_mv;
    
}

uint32_t battery_get_mv(void)
{
    
    return s_last_mv;
}

float battery_get_percent(void)
{
    return battery_mv_to_percent(s_last_mv, s_cells);
}
