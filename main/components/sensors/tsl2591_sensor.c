#include "tsl2591_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <esp_err.h>

static const char *TAG = "TSL2591";

// グローバル設定変数
static tsl2591_config_t current_config = {
    .gain = TSL2591_GAIN_MED,
    .integration = TSL2591_INTEGRATIONTIME_100MS
};

// TSL2591 レジスタ書き込み
static esp_err_t tsl2591_write_register(uint8_t reg, uint8_t value)
{
    uint8_t cmd = TSL2591_COMMAND_BIT | TSL2591_NORMAL_OPERATION | reg;
    uint8_t data[] = {cmd, value};
    
    return i2c_master_write_to_device(I2C_NUM_0, TSL2591_ADDR, data, sizeof(data), pdMS_TO_TICKS(100));
}

// TSL2591 レジスタ読み取り
static esp_err_t tsl2591_read_register(uint8_t reg, uint8_t *value)
{
    uint8_t cmd = TSL2591_COMMAND_BIT | TSL2591_NORMAL_OPERATION | reg;
    
    esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0, TSL2591_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;
    
    return i2c_master_read_from_device(I2C_NUM_0, TSL2591_ADDR, value, 1, pdMS_TO_TICKS(100));
}

// ゲインファクター取得
static float get_gain_factor(tsl2591_gain_t gain)
{
    switch (gain) {
        case TSL2591_GAIN_LOW:  return 1.0f;
        case TSL2591_GAIN_MED:  return 25.0f;
        case TSL2591_GAIN_HIGH: return 400.0f;
        case TSL2591_GAIN_MAX:  return 9900.0f;
        default: return 1.0f;
    }
}

// 積分時間の実際の値（ms）を取得
static float get_integration_time_ms(tsl2591_integration_t integration)
{
    switch (integration) {
        case TSL2591_INTEGRATIONTIME_100MS: return 100.0f;
        case TSL2591_INTEGRATIONTIME_200MS: return 200.0f;
        case TSL2591_INTEGRATIONTIME_300MS: return 300.0f;
        case TSL2591_INTEGRATIONTIME_400MS: return 400.0f;
        case TSL2591_INTEGRATIONTIME_500MS: return 500.0f;
        case TSL2591_INTEGRATIONTIME_600MS: return 600.0f;
        default: return 100.0f;
    }
}

// TSL2591 Lux計算（データシートの推奨計算式）
static float calculate_lux(uint16_t ch0, uint16_t ch1)
{
    if (ch0 == 0) return 0.0f;
    
    float gain_factor = get_gain_factor(current_config.gain);
    float integration_time = get_integration_time_ms(current_config.integration);
    
    // データシートのLux計算式
    float cpl = (integration_time * gain_factor) / 408.0f; // カウント毎ルクス
    float ratio = (float)ch1 / (float)ch0;
    
    float lux;
    if (ratio <= 0.5) {
        lux = (0.0304f * ch0 - 0.062f * ch0 * powf(ratio, 1.4f)) / cpl;
    } else if (ratio <= 0.61) {
        lux = (0.0224f * ch0 - 0.031f * ch1) / cpl;
    } else if (ratio <= 0.80) {
        lux = (0.0128f * ch0 - 0.0153f * ch1) / cpl;
    } else if (ratio <= 1.30) {
        lux = (0.00146f * ch0 - 0.00112f * ch1) / cpl;
    } else {
        lux = 0.0f;
    }
    
    return fmaxf(lux, 0.0f);
}

// TSL2591初期化
esp_err_t tsl2591_init(void)
{
    ESP_LOGI(TAG, "TSL2591センサー初期化中...");
    
    esp_err_t ret;
    uint8_t id;
    
    // デバイスID確認（0x50であることを確認）
    ret = tsl2591_read_register(TSL2591_REGISTER_ID, &id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TSL2591 ID読み取り失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (id != 0x50) {
        ESP_LOGE(TAG, "TSL2591 IDミスマッチ: 期待値 0x50, 実際 0x%02X", id);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "TSL2591 検出完了 ID: 0x%02X", id);
    
    // デバイス有効化
    ret = tsl2591_write_register(TSL2591_REGISTER_ENABLE, 
                                TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TSL2591 有効化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // ゲインと積分時間設定（中感度設定）
    uint8_t config = current_config.gain | current_config.integration;
    ret = tsl2591_write_register(TSL2591_REGISTER_CONFIG, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TSL2591 設定失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "TSL2591 初期化成功");
    return ESP_OK;
}


// TSL2591照度センサー読み取り
esp_err_t tsl2591_read_data(tsl2591_data_t *data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "データポインタがNULLです");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    uint16_t ch0, ch1;
    bool saturated = false;
    int attempts = 4; // 最大4回まで試行（ゲイン設定は4段階のため）

    do {
        // センサーから生データを読み取る
        uint8_t sensor_data[4];
        uint8_t cmd = TSL2591_COMMAND_BIT | TSL2591_NORMAL_OPERATION | TSL2591_REGISTER_C0DATAL;
        ret = i2c_master_write_read_device(I2C_NUM_0, TSL2591_ADDR, &cmd, 1, sensor_data, 4, pdMS_TO_TICKS(200));
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TSL2591: データ読み取り失敗: %s", esp_err_to_name(ret));
            data->error = true;
            return ret;
        }

        ch0 = (sensor_data[1] << 8) | sensor_data[0];
        ch1 = (sensor_data[3] << 8) | sensor_data[2];

        // 飽和チェック
        uint16_t max_count = (current_config.integration == TSL2591_INTEGRATIONTIME_100MS)? 36863 : 65535;
        saturated = (ch0 >= max_count || ch1 >= max_count);

        if (saturated) {
            ESP_LOGW(TAG, "センサー飽和検出！ ゲインを下げて再測定します (ch0=%d, ch1=%d)", ch0, ch1);
            // ゲインを下げる
            if (current_config.gain > TSL2591_GAIN_LOW) {
                // enumの値が0x10ずつ増加することを利用
                tsl2591_gain_t new_gain = (tsl2591_gain_t)(current_config.gain - 0x10);
                
                tsl2591_config_t new_config = {
                    .gain = new_gain,
                    .integration = current_config.integration
                };
                tsl2591_set_config(&new_config); // 新しい設定を適用
                vTaskDelay(pdMS_TO_TICKS(120)); // 新しい積分時間でデータが更新されるのを待つ
            } else {
                // 既に最低ゲインならループを抜ける
                ESP_LOGW(TAG, "最低ゲインでも飽和しています");
                break;
            }
        }
        attempts--;
    } while (saturated && attempts > 0);

    // Lux計算
    data->light_lux = calculate_lux(ch0, ch1);
    data->error = false;
    
    // 照度が低い場合はゲインを上げる（既存の自動ゲイン調整ロジック）
    tsl2591_auto_adjust_gain(ch0);
    
    ESP_LOGI(TAG, "TSL2591 読み取り完了: %.2f Lux", data->light_lux);
        
    return ESP_OK;
}

// TSL2591 自動ゲイン調整
esp_err_t tsl2591_auto_adjust_gain(uint16_t ch0)
{
    // この関数は、照度が低すぎて測定値が小さい場合にゲインを上げる役割に限定する
    
    // 信号が小さすぎるかチェック
    if (ch0 < 100 && current_config.gain < TSL2591_GAIN_MAX) {
        // ゲインを上げる
        tsl2591_gain_t new_gain = (tsl2591_gain_t)(current_config.gain + 0x10);
        if (new_gain > TSL2591_GAIN_MAX) new_gain = TSL2591_GAIN_MAX;

        ESP_LOGI(TAG, "自動ゲイン調整（UP）: %d から %d へ", current_config.gain >> 4, new_gain >> 4);
        
        tsl2591_config_t new_config = {
            .gain = new_gain,
            .integration = current_config.integration
        };
        
        esp_err_t ret = tsl2591_set_config(&new_config);
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(120)); // 新しい設定で安定化
        }
        return ret;
    }
    
    return ESP_OK;
}

// TSL2591設定取得
esp_err_t tsl2591_get_config(tsl2591_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *config = current_config;
    return ESP_OK;
}

// TSL2591設定変更
esp_err_t tsl2591_set_config(const tsl2591_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    current_config = *config;
    
    uint8_t reg_config = current_config.gain | current_config.integration;
    esp_err_t ret = tsl2591_write_register(TSL2591_REGISTER_CONFIG, reg_config);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(200)); // 設定変更後の安定化
        ESP_LOGI(TAG, "設定変更完了: ゲイン=%dx, 積分時間=%dms", 
                 (int)get_gain_factor(current_config.gain),
                 (int)get_integration_time_ms(current_config.integration));
    }
    
    return ret;
}