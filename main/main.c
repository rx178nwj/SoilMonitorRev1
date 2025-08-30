/**
 * @file main.c
 * @brief ESP-IDF v5.5 NimBLE Power-Saving Peripheral for Sensor Data
 *
 * This application demonstrates a BLE peripheral on ESP32-C3 that:
 * 1. Uses the modern NimBLE API for ESP-IDF v5.5.
 * 2. Implements a custom GATT service with a sensor data characteristic (Notify)
 *    and a threshold setting characteristic (Read/Write).
 * 3. Starts a timer on boot that runs independently of BLE connection status.
 * 4. Sends a sensor data notification every 10 seconds ONLY to a subscribed client.
 * 5. Utilizes automatic light sleep with BLE modem sleep to maintain the
 *    connection while minimizing power consumption.
 * 6. Blinks an LED every 10 seconds to indicate timer activity.
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
#include "components/ble/ble_manager.h"          // NimBLEの管理
#include "components/sensors/sht30_sensor.h"               // 温湿度センサー
#include "components/sensors/tsl2591_sensor.h"             // 照度センサー    
#include "wifi_manager.h"               // WiFi管理 
#include "time_sync_manager.h"          // 時刻同期管理
#include "components/sensors/moisture_sensor.h"            // 水分センサー
#include "components/actuators/led_control.h"                // LED制御
#include "common_types.h"               // 共通型定義
#include "components/plant_logic/plant_manager.h"             // 植物管理
#include "nvs_config.h"                // NVS設定
#include "components/plant_logic/data_buffer.h"               // データバッファ

static const char *TAG = "PLANTER_MONITOR";

// NVSキー定義
#define NVS_NAMESPACE "plant_config"
#define NVS_KEY_PROFILE "profile"

/* --- Global Variables --- */
// グローバル変数
// タスクハンドル
static TaskHandle_t g_sensor_task_handle = NULL; // センサーデータ取得タスクのハンドル
static TaskHandle_t g_analysis_task_handle = NULL; // 分析タスクのハンドル


static TimerHandle_t g_notify_timer;


static float g_dry_threshold_mV = 1500.0f; // Default value (mV)


static void notify_timer_callback(TimerHandle_t xTimer);

// I2C初期化
static esp_err_t init_i2c(void)
{
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
static void read_all_sensors(soil_data_t *data)
{

    ESP_LOGI(TAG, "📊 Reading all sensors...");
    
    // 測定時刻を記録
    ESP_LOGI(TAG, "⏰ Getting current time...");
    struct tm datetime;
    time_sync_manager_get_current_time(&datetime);
    // data->datetime = datetime; // Removed: sensor_data_t has no member named 'datetime'
    // sensor_data.sensor_error = false; // Removed: struct has no field 'sensor_error'
    data->datetime = datetime; // soil_data_t has datetime field

    // 水分センサー
    ESP_LOGI(TAG, "🌱 Reading moisture sensor...");
    data->soil_moisture = (float)read_moisture_sensor();
    
    // SHT30温湿度センサー（分離されたモジュールを使用）
    ESP_LOGI(TAG, "🌡️ Reading SHT30 sensor...");
    sht30_data_t sht30;
    esp_err_t ret = sht30_read_data(&sht30);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHT30読み取り失敗: %s", esp_err_to_name(ret));
    } else{
        ESP_LOGI(TAG, "SHT30温度: %.2f °C, 湿度: %.2f %%", 
                 sht30.temperature, sht30.humidity);
        data->temperature = sht30.temperature;
        data->humidity = sht30.humidity;
    }
    
    // TSL2591照度センサー（分離されたモジュールを使用）
    ESP_LOGI(TAG, "💡 Reading TSL2591 sensor...");
    tsl2591_data_t tsl2591;
    ret = tsl2591_read_data(&tsl2591);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TSL2591読み取り失敗: %s", esp_err_to_name(ret));
    } else{
        ESP_LOGI(TAG, "TSL2591照度: %.2f Lux", tsl2591.light_lux);
        data->lux = tsl2591.light_lux;
    }

}

/* --- GPIO Initialization --- */
void init_gpio(void)
{
    gpio_reset_pin(RED_LED_GPIO_PIN);
    gpio_set_direction(RED_LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED_GPIO_PIN, 0);
    ESP_LOGI(TAG, "GPIO%d initialized for RED LED control.", RED_LED_GPIO_PIN);

    gpio_reset_pin(BLU_LED_GPIO_PIN);
    gpio_set_direction(BLU_LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BLU_LED_GPIO_PIN, 0);
    ESP_LOGI(TAG, "GPIO%d initialized for BLUE LED control.", BLU_LED_GPIO_PIN);
}

// センサー読み取り専用タスク
static void sensor_read_task(void* pvParameters)
{
    soil_data_t data;

    while (1) {
        // タイマーからの通知を待機
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        ESP_LOGI(TAG, "📊 Reading all sensors...");
        
        /* [修正箇所] LEDを点灯させ、タイマーが作動したことを示す */
        gpio_set_level(RED_LED_GPIO_PIN, 1);
        
        
        read_all_sensors(&data);

        // センサーデータをデータバッファに保存
        plant_manager_process_sensor_data(&data);

        // 土壌データFIFOにデータを追加
        //if (soil_fifo_push(&g_soil_fifo, &data) != ESP_OK) {
        //    ESP_LOGE(TAG, "Failed to push soil data to FIFO");
        //} else {
        //    ESP_LOGI(TAG, "Soil data pushed to FIFO: Moisture=%d mV, Temp=%.2f °C, Hum=%.2f %%", 
        //             data.soil_moisture, data.temperature, data.humidity);
        //}

        ESP_LOGI(TAG, "Updating sensor data: Temp=%.2f, Hum=%.2f, Lux=%.2f, Soil=%.0f",
                    data.temperature, data.humidity,
                    data.lux, data.soil_moisture);
        
        //display_sensor_data(&data);

        /* 1秒間の点灯制御 */
        vTaskDelay(pdMS_TO_TICKS(1000)); // Keep LED on for a short duration

        /* [修正箇所] 処理完了後、すぐにLEDを消灯する */
        gpio_set_level(RED_LED_GPIO_PIN, 0);
    }
}

/* --- Timer Callback for Notifications --- */
static void notify_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Notify Timer Callback triggered");

    // センサータスクに通知を送信（ブロッキングなし）
    if (g_sensor_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(g_sensor_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

}


// WiFi状態変更コールバック
static void wifi_status_callback(bool connected)
{
    if (connected) {
        ESP_LOGI(TAG, "📶 WiFi接続確立 - 時刻同期を開始します");
        time_sync_manager_start();
    } else {
        ESP_LOGW(TAG, "📶 WiFi接続切断");
    }
}

// 時刻同期完了コールバック
static void time_sync_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "⏰ システム時刻が同期されました");
}

// ネットワーク状態確認
static void check_network_status(void)
{
    // WiFi状態確認
    wifi_manager_check_status();
    
    // 時刻同期状態確認
    time_sync_manager_check_status();
}

// ネットワーク初期化
static void network_init(void)
{
    ESP_LOGI(TAG, "📶 ネットワーク初期化開始...");
    
    // WiFi開始
    esp_err_t ret = wifi_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // WiFi接続を待機（30秒）
    bool wifi_success = wifi_manager_wait_for_connection(WIFI_CONNECT_TIMEOUT_SEC);
    if (wifi_success) {
        ESP_LOGI(TAG, "✅ WiFi接続に成功しました");
        
        // 時刻同期開始（WiFi接続成功時に自動的に開始される）
        if (time_sync_manager_wait_for_sync(SNTP_SYNC_TIMEOUT_SEC)) {
            ESP_LOGI(TAG, "✅ 時刻同期に成功しました");
        } else {
            ESP_LOGW(TAG, "⚠️  時刻同期に失敗 - ローカル時刻を使用");
        }
    } else {
        ESP_LOGW(TAG, "⚠️  WiFi接続に失敗 - オフラインモードで動作");
    }
    
    ESP_LOGI(TAG, "✅ ネットワーク初期化完了");
}

// 初期に時間を取得する
void initial_datetime(void)
{
    // 時間初期化
    /* [修正箇所] 処理完了後、RED/BLU LEDを両方点灯する */
    gpio_set_level(BLU_LED_GPIO_PIN, 1);
    gpio_set_level(RED_LED_GPIO_PIN, 1);

    // 時間を取得する
    // ネットワーク初期化
    network_init();
    
    // 初期化後のネットワーク状態確認
    check_network_status();

    /* [修正箇所] 処理完了後、RED/BLU LEDを両方消灯する */
    gpio_set_level(BLU_LED_GPIO_PIN, 0);
    gpio_set_level(RED_LED_GPIO_PIN, 0);

}

/**
 * メインタスク（システム監視）
 */
static void main_monitoring_task(void *pvParameters) {
    uint32_t heartbeat_count = 0;
    
    ESP_LOGI(TAG, "メイン監視タスク開始");
    
    while (1) {
        heartbeat_count++;
        
        // 5分ごとにハートビートログを出力
        if (heartbeat_count % 5 == 0) {
            ESP_LOGI(TAG, "💓 システムハートビート #%lu (稼働時間: %lu分)", 
                     heartbeat_count, heartbeat_count);
            
            // タスクの健全性チェック
            if (g_sensor_task_handle != NULL && g_analysis_task_handle != NULL) {
                ESP_LOGD(TAG, "全タスク正常稼働中");
            } else {
                ESP_LOGW(TAG, "⚠️ タスクの一部が停止している可能性があります");
            }
        }
        
        // 30分ごとにメモリ情報を表示
        if (heartbeat_count % 30 == 0) {
            ESP_LOGI(TAG, "💾 空きヒープメモリ: %lu bytes", esp_get_free_heap_size());
            ESP_LOGI(TAG, "💾 最小空きヒープメモリ: %lu bytes", esp_get_minimum_free_heap_size());
        }
        
        // 1分待機
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

/**
 * センサーデータと判断結果をログ出力
 */
static void log_sensor_data_and_status(const soil_data_t *soil_data, 
                                     const plant_status_result_t *status, 
                                     int loop_count) {
    // センサーデータを出力
    ESP_LOGI(TAG, "=== 植物状態判断結果 (Loop: %d) ===", loop_count);
    ESP_LOGI(TAG, "現在気温: %.1f℃, 湿度: %.1f%%, 照度: %.0flux, 土壌水分: %.0fmV",
             soil_data->temperature, soil_data->humidity, 
             soil_data->lux, soil_data->soil_moisture);
    
    // 判断結果を出力
    ESP_LOGI(TAG, "生育期: %s", 
             plant_manager_get_growth_phase_string(status->growth_phase));
    ESP_LOGI(TAG, "土壌状態: %s", 
             plant_manager_get_soil_condition_string(status->soil_condition));
}

/**
 * 過去データサマリーをログ出力
 */
static void log_recent_data_summary(void) {
    ESP_LOGI(TAG, "=== 過去データサマリー ===");
    
    
    // 過去7日間の日別サマリーを取得
    daily_summary_data_t daily_summaries[7];
    uint8_t summary_count = 0;
    
    esp_err_t ret = data_buffer_get_recent_daily_summaries(7, daily_summaries, &summary_count);
    if (ret == ESP_OK && summary_count > 0) {
        ESP_LOGI(TAG, "過去%d日間の日別データ:", summary_count);
        for (int i = 0; i < summary_count; i++) {
            ESP_LOGI(TAG, "日%d (%04d-%02d-%02d): 気温%.1f-%.1f℃, 土壌%.0fmV (%d samples)", 
                     i+1,
                     daily_summaries[i].date.tm_year + 1900,
                     daily_summaries[i].date.tm_mon + 1,
                     daily_summaries[i].date.tm_mday,
                     daily_summaries[i].min_temperature, 
                     daily_summaries[i].max_temperature, 
                     daily_summaries[i].avg_soil_moisture,
                     daily_summaries[i].valid_samples);
        }
    } else {
        ESP_LOGI(TAG, "日別データが不足しています");
    }
    
}

/**
 * 植物状態判断と結果表示を行うタスク
 */
static void status_analysis_task(void *pvParameters) {
    int analysis_count = 0;
    
    ESP_LOGI(TAG, "状態分析タスク開始（10分間隔）");
    
    // 初回は少し待機（センサーデータがある程度蓄積されるまで）
    vTaskDelay(pdMS_TO_TICKS(120000)); // 2分待機
    
    while (1) {
        // 植物状態を判断
        plant_status_result_t status = plant_manager_determine_status();
        
        // 最新のセンサーデータを取得
        minute_data_t latest_sensor;
        esp_err_t ret = data_buffer_get_latest_minute_data(&latest_sensor);
        
        if (ret == ESP_OK) {
            // 結果をログ出力（sensor_data_t形式に変換）
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
        
        switch (status.soil_condition)
        {
        case SOIL_DRY:
            // 乾燥時はオレンジLED点灯
            ws2812_set_preset_color(WS2812_COLOR_ORANGE);
            ESP_LOGW(TAG, "⚠️ 土壌が乾燥しています。");
            break;

        case SOIL_MOISTURE_OPTIMAL:
            // 適正は緑LED点灯
            ws2812_set_preset_color(WS2812_COLOR_GREEN);
            ESP_LOGI(TAG, "💧 土壌が湿っています。水やり不要です。");
            break;
        case SOIL_WET:
            // 過湿は青LED点灯
            ws2812_set_preset_color(WS2812_COLOR_BLUE);
            ESP_LOGW(TAG, "⚠️ 土壌が過湿です。水やり不要です。");
            break;
        case NEEDS_WATERING:
            // 水やり必要は赤LED点灯
            ws2812_set_preset_color(WS2812_COLOR_PURPLE);
            ESP_LOGW(TAG, "⚠️ 土壌が非常に乾燥しています。水やりが必要です！");
            break;
        default:
            // 不明は消灯
            ws2812_set_preset_color(WS2812_COLOR_RED);
            ESP_LOGI(TAG, "土壌状態が不明です。");
            break;
        }

        // 10回ごとに詳細情報を表示
        if (analysis_count % 10 == 0) {
            log_recent_data_summary();
            plant_manager_print_system_status();
            
            // 古いデータのクリーンアップ
            data_buffer_cleanup_old_data();
        }
        
        // 10分待機
        vTaskDelay(pdMS_TO_TICKS(60000)); // 10分間隔 ************************
    }
}

/**
 * 植物プロファイル情報をログ出力
 */
static void log_plant_profile(void) {
    const plant_profile_t *profile = plant_manager_get_profile();
    if (profile == NULL) {
        ESP_LOGE(TAG, "Failed to get plant profile");
        return;
    }
    
    ESP_LOGI(TAG, "=== 植物プロファイル情報 ===");
    ESP_LOGI(TAG, "植物名: %s", profile->plant_name);
    ESP_LOGI(TAG, "高温休眠: 最高%.1f℃以上 or 最低%.1f℃以上が%d日", 
             profile->high_temp_dormancy_max_temp,
             profile->high_temp_dormancy_min_temp,
             profile->high_temp_dormancy_min_temp_days);
    ESP_LOGI(TAG, "低温休眠: 最低%.1f℃以下", profile->low_temp_dormancy_min_temp);
    ESP_LOGI(TAG, "活動期: %.1f-%.1f℃が%d日連続", 
             profile->active_period_min_temp,
             profile->active_period_max_temp,
             profile->active_period_consecutive_days);
    ESP_LOGI(TAG, "土壌: 乾燥%.0fmV以上, 適正%.0f-%.0fmV, 水やり%d日", 
             profile->soil_dry_threshold,
             profile->soil_moisture_opt_min,
             profile->soil_moisture_opt_max,
             profile->soil_dry_days_for_watering);
}

// システム初期化
static esp_err_t system_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "🔄 システム初期化開始...");
    
    // NVSフラッシュとNimBLEホストスタックを初期化する
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // switch入力初期化
    ESP_LOGI(TAG, "🔄 スイッチ入力初期化...");
    switch_input_init();

    // ADCの初期化
    ESP_LOGI(TAG, "🔄 ADCの初期化...");
    init_adc();

    // i2CとGPIOの初期化
    ESP_LOGI(TAG, "🔄 I2CとGPIOの初期化...");
    init_i2c();

    // GPIOの初期化
    ESP_LOGI(TAG, "🔄 GPIOの初期化...");
    init_gpio();

    // LED制御システム初期化
    ESP_LOGI(TAG, "🔄 LED制御システム初期化...");
    ret = led_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED control initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // SHT30センサー初期化
    ESP_LOGI(TAG, "🔄 SHT30センサー初期化...");
    ret = sht30_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHT30 initialization failed - temperature/humidity sensor disabled");
    } else {
        ESP_LOGI(TAG, "✅ SHT30 temperature/humidity sensor initialized successfully");
    }
    
    // TSL2591センサー初期化
    ESP_LOGI(TAG, "🔄 TSL2591センサー初期化...");
    ret = tsl2591_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TSL2591 initialization failed - light sensor disabled");
    } else {
        ESP_LOGI(TAG, "✅ TSL2591 light sensor initialized successfully");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C3 植物管理システム 初期化開始");
    ESP_LOGI(TAG, "データバッファリング機能付き");
    ESP_LOGI(TAG, "========================================");
    
    // 植物管理システムを初期化（内部でNVSとデータバッファも初期化）
    ESP_LOGI(TAG, "植物管理システムを初期化中...");
    ret = plant_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "植物管理システムの初期化に失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ 植物管理システム初期化完了");
    
    // 植物プロファイル情報を出力
    log_plant_profile();
    
    // WiFi管理システム初期化
    int i;
    for(i=0;i<3;i++){
        switch(i){
        case 0:
            // 1回目はデフォルトSSID/PWを使用
            strncpy((char*)g_wifi_config.sta.ssid, WIFI_SSID, sizeof(g_wifi_config.sta.ssid) - 1);
            strncpy((char*)g_wifi_config.sta.password, WIFI_PASSWORD, sizeof(g_wifi_config.sta.password) - 1);
            break;
        case 1:
            // 2回目は予備SSID/PASSを使用
            strncpy((char*)g_wifi_config.sta.ssid, WIFI_SSID1, sizeof(g_wifi_config.sta.ssid) - 1);
            strncpy((char*)g_wifi_config.sta.password, WIFI_PASSWORD1, sizeof(g_wifi_config.sta.password) - 1);
        case 2:
            // 2回目は予備SSID/PASSを使用
            strncpy((char*)g_wifi_config.sta.ssid, WIFI_SSID2, sizeof(g_wifi_config.sta.ssid) - 1);
            strncpy((char*)g_wifi_config.sta.password, WIFI_PASSWORD2, sizeof(g_wifi_config.sta.password) - 1);
        }
        ESP_LOGI(TAG, "🔄 WiFi設定: SSID='%s'", g_wifi_config.sta.ssid);
        ESP_LOGI(TAG, "🔄 WiFi管理システム初期化... (試行 %d/3)", i+1);
        ret = wifi_manager_init(wifi_status_callback);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "WiFi manager initialization attempt %d failed: %s", i+1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2秒待機してから再試行
    }
    
    ret = wifi_manager_init(wifi_status_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 時刻同期管理システム初期化
    ret = time_sync_manager_init(time_sync_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Time sync manager initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // データバッファの初期化
    ESP_LOGI(TAG, "🔄 データバッファの初期化...");
    data_buffer_init(); // データバッファの初期化
    
    ESP_LOGI(TAG, "✅ システム初期化完了");
    return ESP_OK;
}

/* --- Main Application Entry --- */
void app_main(void)
{
    esp_err_t ret;

    // 起動直後2秒間停止する
    vTaskDelay(pdMS_TO_TICKS(2000)); // Keep LED on for a short duration

    // Initialize the system
    ESP_LOGI(TAG, "Starting Soil Monitor Application...");
    ESP_ERROR_CHECK(system_init());

#ifdef CONFIG_PM_ENABLE
    // Power management configuration
    ESP_LOGI(TAG, "🔄 Power management configuration...");
    esp_pm_config_t pm_config = {
     .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
     .min_freq_mhz = 10,
     .light_sleep_enable = true
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Power management enabled");
#endif

    ble_manager_init(); // NimBLEの初期化

    // 時間取得
    ESP_LOGI(TAG, "🔄 Initializing time and network...");
    initial_datetime();

    // 土壌データFIFOの初期化
    //ESP_LOGI(TAG, "🔄 Initializing soil data FIFO...");
    //ESP_ERROR_CHECK(soil_fifo_init(&g_soil_fifo, 60*24)); // 1日分のデータを保持
    //if (g_soil_fifo.buffer == NULL) {
    //    ESP_LOGE(TAG, "Failed to initialize soil data FIFO");
    //    return;
    //}
    // センサータスク作成（十分なスタックサイズ）
    xTaskCreate(sensor_read_task, "sensor_read", 4096, NULL, 5, &g_sensor_task_handle);
    
    // 状態分析タスクを作成（10分間隔）
    BaseType_t analysis_task_ret = xTaskCreate(
        status_analysis_task,       // タスク関数
        "analysis_task",            // タスク名
        6144,                       // スタックサイズ（大きめ）
        NULL,                       // パラメータ
        4,                          // 優先度（センサータスクより少し低く）
        &g_analysis_task_handle     // タスクハンドル
    );
    
    if (analysis_task_ret != pdPASS) {
        ESP_LOGE(TAG, "状態分析タスクの作成に失敗");
        return ;
    }
    ESP_LOGI(TAG, "✓ 状態分析タスク作成完了");

    // タイマータスクを設定します
    ESP_LOGI(TAG, "🔄 Creating notification timer...");
    g_notify_timer = xTimerCreate("notify_timer", pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS), pdTRUE, NULL, notify_timer_callback);
    if (g_notify_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return;
    }

    /* [修正箇所] アプリケーション起動直後にタイマーを開始します */
    if (xTimerStart(g_notify_timer, 0)!= pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return;
    }
    ESP_LOGI(TAG, "Notification timer started on boot.");

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "Initialization complete.");
}