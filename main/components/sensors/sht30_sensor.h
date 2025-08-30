#ifndef SHT30_SENSOR_H
#define SHT30_SENSOR_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// SHT30定数定義
#define SHT30_ADDR          0x45         // SHT30のI2Cアドレス（ADDR pin = VDD）

// SHT30センサーデータ構造体
typedef struct {
    float temperature;
    float humidity;
    bool error;
} sht30_data_t;

// SHT30初期化
esp_err_t sht30_init(void);

// SHT30温湿度読み取り
esp_err_t sht30_read_data(sht30_data_t *data);

// SHT30ソフトリセット
esp_err_t sht30_soft_reset(void);

// CRC-8計算関数
uint8_t sht30_calculate_crc(uint8_t *data, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif // SHT30_SENSOR_H