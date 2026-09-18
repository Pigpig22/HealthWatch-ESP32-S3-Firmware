#ifndef NTC_H
#define NTC_H

#include <stdbool.h>

#define NTC_ADC_CHANNEL     ADC1_CHANNEL_5
#define NTC_DO_PIN          7

#define NTC_SERIES_RES      6550.0f
#define NTC_NOMINAL_RES     5300.0f
#define NTC_B_COEFF         3950.0f
#define NTC_NOMINAL_TEMP    25.0f

#define TEMP_LOW_THRESHOLD  36.0f
#define TEMP_NORMAL_MAX     37.2f
#define TEMP_HIGH_MAX       37.9f

typedef struct {
    int adc_channel;
    int do_pin;
    float series_res;
    float nominal_res;
    float b_coeff;
    float nominal_temp;
} ntc_config_t;

typedef enum {
    TEMP_LOW = 0,
    TEMP_NORMAL,
    TEMP_HIGH,
    TEMP_FEVER
} temp_status_t;

ntc_config_t ntc_get_default_config(void);
void ntc_init(ntc_config_t *config);
int ntc_read_adc_raw(ntc_config_t *config);
float ntc_read_voltage(ntc_config_t *config);
float ntc_read_resistance(ntc_config_t *config);
float ntc_read_temperature(ntc_config_t *config);
float ntc_read_temperature_avg(ntc_config_t *config, int samples);
bool ntc_read_alarm(ntc_config_t *config);
temp_status_t ntc_get_temp_status(float temperature);
const char* ntc_get_temp_status_string(temp_status_t status);
void ntc_calibrate(ntc_config_t *config, float reference_temp);

#endif