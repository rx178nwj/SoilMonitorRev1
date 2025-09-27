/**
 * @file main.c
 * @brief ESP-IDF v5.5 NimBLE Power-Saving Peripheral for Sensor Data
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include <esp_err.h>

/* NimBLE Includes */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "esp_bt.h"

// I2C and GPIO and ADC includes
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// 分離されたモジュール
#include "components/ble/ble_manager.h"
#include "components/sensors/sht30_sensor.h"
#include "components/sensors/tsl2591_sensor.h"
#include "wifi_manager.h"
#include "time_sync_manager.h"
#include "components/sensors/moisture_sensor.h"
#include "components/actuators/led_control.h"
#include "common_types.h"
#include "components/plant_logic/plant_manager.h"
#include "nvs_config.h"
#include "components/plant_logic/data_buffer.h"

static const char *TAG = "PLANTER_MONITOR";

// タスクハンドル
static TaskHandle_t g_sensor_task_handle = NULL;
static TaskHandle_t g_analysis_task_handle = NULL;

static TimerHandle_t g_notify_timer;

static void notify_timer_callback(TimerHandle_t xTimer);

// I2C初期化
static esp_err_t init_i2c(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000, // 100kHz
        .clk_flags = 0,
    };
    esp_err_t ret = i2c_param_config(I2C_NUM_0, &i2c_config);
    if (ret != ESP_OK) return ret;
    ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "I2C initialized successfully");
    }
    return ret;
}

// 全センサーデータ読み取り
static void read_all_sensors(soil_data_t *data) {
    ESP_LOGI(TAG, "📊 Reading all sensors...");
    struct tm datetime;
    time_sync_manager_get_current_time(&datetime);
    data->datetime = datetime;

    data->soil_moisture = (float)read_moisture_sensor();

    sht30_data_t sht30;
    if (sht30_read_data(&sht30) == ESP_OK) {
        data->temperature = sht30.temperature;
        data->humidity = sht30.humidity;
    }

    tsl2591_data_t tsl2591;
    if (tsl2591_read_data(&tsl2591) == ESP_OK) {
        data->lux = tsl2591.light_lux;
    }
}

/* --- GPIO Initialization --- */
void init_gpio(void) {
    gpio_reset_pin(RED_LED_GPIO_PIN);
    gpio_set_direction(RED_LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED_GPIO_PIN, 0);

    gpio_reset_pin(BLU_LED_GPIO_PIN);
    gpio_set_direction(BLU_LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BLU_LED_GPIO_PIN, 0);
}

// センサー読み取り専用タスク
static void sensor_read_task(void* pvParameters) {
    soil_data_t data;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        gpio_set_level(RED_LED_GPIO_PIN, 1);
        read_all_sensors(&data);
        plant_manager_process_sensor_data(&data);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(RED_LED_GPIO_PIN, 0);
    }
}

/* --- Timer Callback for Notifications --- */
static void notify_timer_callback(TimerHandle_t xTimer) {
    if (g_sensor_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(g_sensor_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// WiFi/Timeコールバック
static void wifi_status_callback(bool connected) {
    if (connected) time_sync_manager_start();
}
static void time_sync_callback(struct timeval *tv) {
    ESP_LOGI(TAG, "⏰ システム時刻が同期されました");
}

// ネットワーク初期化
static void network_init(void) {
    wifi_manager_start();
    if (wifi_manager_wait_for_connection(WIFI_CONNECT_TIMEOUT_SEC)) {
        time_sync_manager_wait_for_sync(SNTP_SYNC_TIMEOUT_SEC);
    }
}

// センサーデータと判断結果をログ出力
static void log_sensor_data_and_status(const soil_data_t *soil_data,
                                     const plant_status_result_t *status,
                                     int loop_count) {
    ESP_LOGI(TAG, "=== 植物状態判断結果 (Loop: %d) ===", loop_count);
    ESP_LOGI(TAG, "現在気温: %.1f℃, 湿度: %.1f%%, 照度: %.0flux, 土壌水分: %.0fmV",
             soil_data->temperature, soil_data->humidity,
             soil_data->lux, soil_data->soil_moisture);
    ESP_LOGI(TAG, "状態: %s",
             plant_manager_get_plant_condition_string(status->plant_condition));
}

/**
 * 植物状態判断と結果表示を行うタスク
 */
static void status_analysis_task(void *pvParameters) {
    int analysis_count = 0;
    ESP_LOGI(TAG, "状態分析タスク開始（1分間隔）");
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒待機

    while (1) {
        plant_status_result_t status = plant_manager_determine_status();
        minute_data_t latest_sensor;

        if (data_buffer_get_latest_minute_data(&latest_sensor) == ESP_OK) {
            soil_data_t display_data = {
                .datetime = latest_sensor.timestamp,
                .temperature = latest_sensor.temperature,
                .humidity = latest_sensor.humidity,
                .lux = latest_sensor.lux,
                .soil_moisture = latest_sensor.soil_moisture
            };
            log_sensor_data_and_status(&display_data, &status, ++analysis_count);
        } else {
            ESP_LOGW(TAG, "最新センサーデータの取得に失敗");
        }

        switch (status.plant_condition) {
            case TEMP_TOO_HIGH:
                ws2812_set_preset_color(WS2812_COLOR_RED);
                ESP_LOGW(TAG, "🔥 高温限界です！");
                break;
            case TEMP_TOO_LOW:
                ws2812_set_preset_color(WS2812_COLOR_BLUE);
                ESP_LOGW(TAG, "🧊 低温限界です！");
                break;
            case NEEDS_WATERING:
                ws2812_set_preset_color(WS2812_COLOR_YELLOW);
                ESP_LOGW(TAG, "💧 灌水が必要です！");
                break;
            case SOIL_DRY:
                ws2812_set_preset_color(WS2812_COLOR_ORANGE);
                break;
            case SOIL_WET:
                ws2812_set_preset_color(WS2812_COLOR_GREEN);
                break;
            case WATERING_COMPLETED:
                ws2812_set_preset_color(WS2812_COLOR_WHITE);
                break;
            default:
                ws2812_set_preset_color(WS2812_COLOR_OFF);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); // 1分待機
    }
}

/**
 * 植物プロファイル情報をログ出力
 */
static void log_plant_profile(void) {
    const plant_profile_t *profile = plant_manager_get_profile();
    if (profile == NULL) return;

    ESP_LOGI(TAG, "=== 植物プロファイル情報 ===");
    ESP_LOGI(TAG, "植物名: %s", profile->plant_name);
    ESP_LOGI(TAG, "土壌: 乾燥>=%.0fmV, 湿潤<=%.0fmV, 灌水要求%d日",
             profile->soil_dry_threshold,
             profile->soil_wet_threshold,
             profile->soil_dry_days_for_watering);
    ESP_LOGI(TAG, "気温限界: 高温>=%.1f℃, 低温<=%.1f℃",
             profile->temp_high_limit,
             profile->temp_low_limit);
}

// システム初期化
static esp_err_t system_init(void) {
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    switch_input_init();
    init_adc();
    init_i2c();
    init_gpio();
    led_control_init();
    sht30_init();
    tsl2591_init();

    ESP_ERROR_CHECK(plant_manager_init());
    log_plant_profile();

    // WiFiと時刻同期の初期化
    ESP_ERROR_CHECK(wifi_manager_init(wifi_status_callback));
    ESP_ERROR_CHECK(time_sync_manager_init(time_sync_callback));
    
    data_buffer_init();
    return ESP_OK;
}

/* --- Main Application Entry --- */
void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting Soil Monitor Application...");
    ESP_ERROR_CHECK(system_init());

#ifdef CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
     .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
     .min_freq_mhz = 10,
     .light_sleep_enable = true
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
#endif

    ble_manager_init();
    network_init();

    xTaskCreate(sensor_read_task, "sensor_read", 4096, NULL, 5, &g_sensor_task_handle);
    xTaskCreate(status_analysis_task, "analysis_task", 6144, NULL, 4, &g_analysis_task_handle);

    g_notify_timer = xTimerCreate("notify_timer", pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS), pdTRUE, NULL, notify_timer_callback);
    xTimerStart(g_notify_timer, 0);

    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "Initialization complete.");
}
