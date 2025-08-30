/**
 * @file soil_gatt_service.h
 * @brief 土壌監視専用GATTサービス
 * 
 * センサーデータ送信とステータス情報提供のためのBLEサービス
 */

#ifndef SOIL_GATT_SERVICE_H
#define SOIL_GATT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "host/ble_gatt.h"
#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- GATT サービス/キャラクタリスティック識別子 --- */
typedef enum {
    SOIL_CHAR_SENSOR_DATA = 0,  // センサーデータ (Notify/Read)
    SOIL_CHAR_DATA_STATUS,      // データステータス (Read/Write)
    SOIL_CHAR_MAX
} soil_characteristic_id_t;

/* --- データアクセスコールバック型定義 --- */
typedef esp_err_t (*soil_data_read_callback_t)(soil_ble_data_t *data);
typedef esp_err_t (*soil_status_read_callback_t)(ble_data_status_t *status);
typedef esp_err_t (*soil_status_write_callback_t)(const ble_data_status_t *status);

/* --- GATT サービス設定構造体 --- */
typedef struct {
    soil_data_read_callback_t data_read_cb;     // センサーデータ読み取りコールバック
    soil_status_read_callback_t status_read_cb; // ステータス読み取りコールバック
    soil_status_write_callback_t status_write_cb; // ステータス書き込みコールバック
} soil_gatt_callbacks_t;

/* --- パブリック関数 --- */

/**
 * @brief 土壌監視GATTサービス初期化
 * @param callbacks データアクセスコールバック構造体
 * @return ESP_OK: 成功, その他: エラー
 */
esp_err_t soil_gatt_service_init(const soil_gatt_callbacks_t *callbacks);

/**
 * @brief GATTサービス登録
 * @return ESP_OK: 成功, その他: エラー
 */
esp_err_t soil_gatt_service_register(void);

/**
 * @brief センサーデータ通知送信
 * @param data 送信するセンサーデータ
 * @return ESP_OK: 成功, その他: エラー
 */
esp_err_t soil_gatt_service_notify_sensor_data(const soil_ble_data_t *data);

/**
 * @brief 指定したキャラクタリスティックのハンドル取得
 * @param char_id キャラクタリスティック識別子
 * @return uint16_t キャラクタリスティックハンドル (0: 無効)
 */
uint16_t soil_gatt_service_get_char_handle(soil_characteristic_id_t char_id);

/**
 * @brief センサーデータキャラクタリスティックがサブスクライブされているか確認
 * @return true: サブスクライブ済み, false: 未サブスクライブ
 */
bool soil_gatt_service_is_sensor_data_subscribed(void);

/**
 * @brief GATTサービス終了処理
 */
void soil_gatt_service_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SOIL_GATT_SERVICE_H */