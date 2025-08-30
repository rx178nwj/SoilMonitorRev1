#include "sht30_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SHT30";

// SHT30温湿度センサー読み取り
esp_err_t sht30_read_data(sht30_data_t *data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "データポインタがNULLです");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd[] = {0x2C, 0x06}; // High repeatability measurement with clock stretching enabled
    uint8_t sensor_data[6];
    
    ESP_LOGD(TAG, "SHT30: 測定コマンド送信");
    
    // コマンド送信
    esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0, SHT30_ADDR, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT30: コマンド書き込み失敗: %s", esp_err_to_name(ret));
        data->error = true;
        return ret;
    }
    
    // 測定完了まで待機（高精度モードは最大15ms）
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // データ読み取り（6バイト: 温度2バイト + CRC1バイト + 湿度2バイト + CRC1バイト）
    ret = i2c_master_read_from_device(I2C_NUM_0, SHT30_ADDR, sensor_data, sizeof(sensor_data), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT30: データ読み取り失敗: %s", esp_err_to_name(ret));
        data->error = true;
        return ret;
    }
    
    ESP_LOGD(TAG, "SHT30: 生データ: %02X %02X %02X %02X %02X %02X", 
             sensor_data[0], sensor_data[1], sensor_data[2], sensor_data[3], sensor_data[4], sensor_data[5]);
    
    // CRCチェック（温度データ）
    uint8_t temp_crc = sht30_calculate_crc(&sensor_data[0], 2);
    if (temp_crc != sensor_data[2]) {
        ESP_LOGW(TAG, "SHT30: 温度CRCミスマッチ. 期待値: 0x%02X, 実際: 0x%02X", temp_crc, sensor_data[2]);
    }
    
    // CRCチェック（湿度データ）
    uint8_t hum_crc = sht30_calculate_crc(&sensor_data[3], 2);
    if (hum_crc != sensor_data[5]) {
        ESP_LOGW(TAG, "SHT30: 湿度CRCミスマッチ. 期待値: 0x%02X, 実際: 0x%02X", hum_crc, sensor_data[5]);
    }
    
    // データ変換（データシート p.14の公式に従う）
    uint16_t temp_raw = (sensor_data[0] << 8) | sensor_data[1];
    uint16_t hum_raw = (sensor_data[3] << 8) | sensor_data[4];
    
    // 温度変換: T[°C] = -45 + 175 * (ST / (2^16 - 1))
    data->temperature = -45.0f + 175.0f * ((float)temp_raw / 65535.0f);
    
    // 湿度変換: RH[%] = 100 * (SRH / (2^16 - 1))
    data->humidity = 100.0f * ((float)hum_raw / 65535.0f);
    
    data->error = false;
    
    ESP_LOGD(TAG, "SHT30: 温度: %.2f°C, 湿度: %.2f%%", data->temperature, data->humidity);
    
    return ESP_OK;
}

// CRC-8計算関数（データシート p.14の仕様に従う）
uint8_t sht30_calculate_crc(uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFF; // Initialization: 0xFF
    
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // Polynomial: 0x31 (x8 + x5 + x4 + 1)
            } else {
                crc = (crc << 1);
            }
        }
    }
    
    return crc; // Final XOR: 0x00 (no XOR)
}

// SHT30ソフトリセット関数
esp_err_t sht30_soft_reset(void)
{
    uint8_t cmd[] = {0x30, 0xA2}; // Soft reset command
    
    ESP_LOGI(TAG, "SHT30: ソフトリセット実行");
    
    esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0, SHT30_ADDR, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT30: ソフトリセット失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // リセット後の待機時間
    vTaskDelay(pdMS_TO_TICKS(2));
    
    ESP_LOGI(TAG, "SHT30: ソフトリセット完了");
    return ESP_OK;
}

// SHT30初期化関数
esp_err_t sht30_init(void)
{
    ESP_LOGI(TAG, "SHT30センサー初期化中...");
    
    // ソフトリセット実行
    esp_err_t ret = sht30_soft_reset();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHT30: ソフトリセット失敗、初期化を継続");
    }
    
    // テスト測定を実行して接続確認
    sht30_data_t test_data;
    ret = sht30_read_data(&test_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT30: テスト測定失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 測定値の妥当性チェック
    if (test_data.temperature < -40.0f || test_data.temperature > 125.0f || 
        test_data.humidity < 0.0f || test_data.humidity > 100.0f) {
        ESP_LOGW(TAG, "SHT30: テスト測定値が範囲外 (T:%.1f°C, H:%.1f%%)", 
                 test_data.temperature, test_data.humidity);
    }
    
    ESP_LOGI(TAG, "SHT30: 初期化成功 (T:%.1f°C, H:%.1f%%)", 
             test_data.temperature, test_data.humidity);
    return ESP_OK;
}