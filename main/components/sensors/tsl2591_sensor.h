#ifndef TSL2591_SENSOR_H
#define TSL2591_SENSOR_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// TSL2591定数定義
#define TSL2591_ADDR                0x29         // TSL2591のI2Cアドレス（データシート準拠）

// TSL2591 レジスタ定義（データシート準拠）
#define TSL2591_COMMAND_BIT         (0x80)
#define TSL2591_NORMAL_OPERATION    (0x20)
#define TSL2591_REGISTER_ENABLE     (0x00)
#define TSL2591_REGISTER_CONFIG     (0x01)
#define TSL2591_REGISTER_AILTL      (0x04)
#define TSL2591_REGISTER_AILTH      (0x05)
#define TSL2591_REGISTER_AIHTL      (0x06)
#define TSL2591_REGISTER_AIHTH      (0x07)
#define TSL2591_REGISTER_NPAILTL    (0x08)
#define TSL2591_REGISTER_NPAILTH    (0x09)
#define TSL2591_REGISTER_NPAIHTL    (0x0A)
#define TSL2591_REGISTER_NPAIHTH    (0x0B)
#define TSL2591_REGISTER_PERSIST    (0x0C)
#define TSL2591_REGISTER_PID        (0x11)
#define TSL2591_REGISTER_ID         (0x12)
#define TSL2591_REGISTER_STATUS     (0x13)
#define TSL2591_REGISTER_C0DATAL    (0x14)
#define TSL2591_REGISTER_C0DATAH    (0x15)
#define TSL2591_REGISTER_C1DATAL    (0x16)
#define TSL2591_REGISTER_C1DATAH    (0x17)

// TSL2591 設定値
#define TSL2591_ENABLE_POWERON      (0x01)
#define TSL2591_ENABLE_AEN          (0x02)
#define TSL2591_ENABLE_AIEN         (0x10)
#define TSL2591_ENABLE_NPIEN        (0x80)

// ゲイン設定（データシート準拠）
typedef enum {
    TSL2591_GAIN_LOW  = 0x00,    // 1x ゲイン
    TSL2591_GAIN_MED  = 0x10,    // 25x ゲイン  
    TSL2591_GAIN_HIGH = 0x20,    // 400x ゲイン
    TSL2591_GAIN_MAX  = 0x30     // 9900x ゲイン
} tsl2591_gain_t;

// 積分時間設定（データシート準拠）
typedef enum {
    TSL2591_INTEGRATIONTIME_100MS = 0x00,
    TSL2591_INTEGRATIONTIME_200MS = 0x01,
    TSL2591_INTEGRATIONTIME_300MS = 0x02,
    TSL2591_INTEGRATIONTIME_400MS = 0x03,
    TSL2591_INTEGRATIONTIME_500MS = 0x04,
    TSL2591_INTEGRATIONTIME_600MS = 0x05
} tsl2591_integration_t;

// TSL2591センサーデータ構造体
typedef struct {
    float light_lux;
    bool error;
} tsl2591_data_t;

// TSL2591設定構造体
typedef struct {
    tsl2591_gain_t gain;
    tsl2591_integration_t integration;
} tsl2591_config_t;

// TSL2591初期化
esp_err_t tsl2591_init(void);

// TSL2591照度読み取り
esp_err_t tsl2591_read_data(tsl2591_data_t *data);

// TSL2591設定取得
esp_err_t tsl2591_get_config(tsl2591_config_t *config);

// TSL2591設定変更
esp_err_t tsl2591_set_config(const tsl2591_config_t *config);

// 自動ゲイン調整
esp_err_t tsl2591_auto_adjust_gain(uint16_t ch0);

#ifdef __cplusplus
}
#endif

#endif // TSL2591_SENSOR_H