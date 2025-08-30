#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <time.h>
#include <stdint.h>
#include "host/ble_hs.h" // ble_gap_event のためにインクルード

/* --- Command and Response Data Structures --- */

// コマンドパケット
typedef struct __attribute__((packed)) {
    uint8_t command_id;     // コマンド識別子
    uint8_t sequence_num;   // シーケンス番号
    uint16_t data_length;   // データ長
    uint8_t data[];         // コマンドデータ
} ble_command_packet_t;

// レスポンスパケット
typedef struct __attribute__((packed)) {
    uint8_t response_id;    // レスポンス識別子
    uint8_t status_code;    // ステータスコード
    uint8_t sequence_num;   // 対応するシーケンス番号
    uint16_t data_length;   // レスポンスデータ長
    uint8_t data[];         // レスポンスデータ
} ble_response_packet_t;

// 時間指定リクエスト用構造体
typedef struct __attribute__((packed)) {
    struct tm requested_time; // 要求する時間
} time_data_request_t;

// 時間指定データ取得レスポンス用構造体
typedef struct __attribute__((packed)) {
    struct tm actual_time;    // 実際に見つかったデータの時間
    float temperature;        // 気温
    float humidity;           // 湿度
    float lux;                // 照度
    float soil_moisture;      // 土壌水分
} time_data_response_t;

// デバイス情報構造体
typedef struct __attribute__((packed)) {
    char device_name[32];
    char firmware_version[16];
    char hardware_version[16];
    uint32_t uptime_seconds;
    uint32_t total_sensor_readings;
} device_info_t;

/* --- Command and Response Enums --- */

typedef enum {
    CMD_GET_SENSOR_DATA = 0x01,     // 最新センサーデータ取得
    CMD_GET_SYSTEM_STATUS = 0x02,   // システム状態取得（メモリ使用量、稼働時間等）
    CMD_SET_THRESHOLD = 0x03,       // 土壌水分しきい値設定
    CMD_GET_HISTORY_DATA = 0x04,    // 履歴データ取得
    CMD_SYSTEM_RESET = 0x05,        // システムリセット
    CMD_GET_DEVICE_INFO = 0x06,     // デバイス情報取得（名前、FWバージョン等）
    CMD_SET_TIME = 0x07,            // 時刻設定
    CMD_GET_CONFIG = 0x08,          // 設定取得
    CMD_SET_CONFIG = 0x09,          // 設定変更
    CMD_GET_TIME_DATA = 0x0A,       // 指定時間データ取得
    CMD_GET_SWITCH_STATUS = 0x0B,   // スイッチ状態取得
} ble_command_id_t;

typedef enum {
    RESP_STATUS_SUCCESS = 0x00,
    RESP_STATUS_ERROR = 0x01,
    RESP_STATUS_INVALID_COMMAND = 0x02,
    RESP_STATUS_INVALID_PARAMETER = 0x03,
    RESP_STATUS_BUSY = 0x04,
    RESP_STATUS_NOT_SUPPORTED = 0x05,
} ble_response_status_t;


/* --- Public Function Prototypes --- */

void ble_manager_init(void);    // BLEマネージャー初期化
void ble_host_task(void *param); // BLEホストタスク
void print_ble_system_info(void); // BLEシステム情報を表示

// BLE GATT Access Callback
static int gatt_svr_access_sensor_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
// BLE GATT Access Callback
static int gatt_svr_access_data_status_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_handler(struct ble_gap_event *event, void *arg);
void ble_manager_init(void);    // BLEマネージャー初期化

void ble_host_task(void *param); // BLEホストタスク
void start_advertising(void);   // 広告開始
static void on_reset(int reason); // リセット時のコールバック
static void on_sync(void);      // 同期時のコールバック


#endif // BLE_MANAGER_H 