#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/i2c.h"

#include "moisture_sensor.h"
#include <esp_err.h>

// TAG for logging
static const char *TAG = "PLANTER_ADC";

// ADC設定
#define ADC_ATTEN           ADC_ATTEN_DB_12 // 12dBの減衰を使用
#define ADC_BITWIDTH        ADC_BITWIDTH_12 // 12ビットの分解能

// グローバル変数
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_chan2_handle = NULL;

// ADC初期化
void init_adc(void)
{
    // ADC1初期化
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // ADC1チャンネル設定
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));

    // ADCキャリブレーション (ESP32-C3に適合するLine Fitting方式に変更)
    ESP_LOGI(TAG, "ADC-Calibration: Using Line Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
        // Note: .chan is not a member of line_fitting_config_t
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_chan2_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "ADC calibration scheme not supported, using raw values");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values");
    }
    
    ESP_LOGI(TAG, "ADC initialized for moisture sensor");
}


// 水分センサー読み取り
uint16_t read_moisture_sensor(void)
{
    int adc_raw;
    int voltage = 0;
    
    for (int i = 0; i < 10; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &adc_raw));
        
        if (adc1_cali_chan2_handle) {
            int vol_mv;
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan2_handle, adc_raw, &vol_mv));
            voltage += vol_mv;
        } else {
            voltage += adc_raw; // キャリブレーション失敗時はRAW値を使用
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return voltage / 10; // 10回平均
}