/**
 * @file soil_gatt_service.c
 * @brief 土壌監視GATTサービス実装
 */

#include "soil_gatt_service.h"
#include "../ble/ble_manager.h"
#include <string.h>
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "SOIL_GATT";

/* --- カスタムUUID定義 --- */
// カスタムサービスUUID: 59462f12-9543-9999-12c8-58b459a2712d
static const ble_uuid128_t s_service_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

// センサーデータキャラクタリスティックUUID: 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456789
static const ble_uuid128_t s_sensor_data_char_uuid =
    BLE_UUID128_INIT(0x89, 0x67, 0x45, 0x23, 0xf1, 0xe0, 0x9d, 0x8c,
                     0x7b, 0x6a, 0x5f, 0x4e, 0x1d, 0x2c, 0x3b, 0x6a);

// データステータスキャラクタリスティックUUID: 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456790
static const ble_uuid128_t s_data_status_char_uuid =
    BLE_UUID128_INIT(0x90, 0x67, 0x45, 0x23, 0xf1, 0xe0, 0x9d, 0x8c,
                     0x7b, 0x6a, 0x5f, 0x4e, 0x1d, 0x2c, 0x3b, 0x6a);

/* --- プライベート変数 --- */
static soil_gatt_callbacks_t s_callbacks = {0};
static uint16_t s_char_handles[SOIL_CHAR_MAX] = {0};
static bool s_initialized = false;

/* --- プライベート関数プロトタイプ --- */
static int sensor_data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int data_status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

/* --- GATT サービス定義 --- */
static const struct ble_gatt_svc_def s_gatt_service_def[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // センサーデータキャラクタリスティック (Read/Notify)
                .uuid = &s_sensor_data_char_uuid.u,
                .access_cb = sensor_data_access_cb,
                .val_handle = &s_char_handles[SOIL_CHAR_SENSOR_DATA],
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                // データステータスキャラクタリスティック (Read/Write)
                .uuid = &s_data_status_char_uuid.u,
                .access_cb = data_status_access_cb,
                .val_handle = &s_char_handles[SOIL_CHAR_DATA_STATUS],
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {0} /* キャラクタリスティック終端 */
        },
    },
    {0} /* サービス終端 */
};

/* --- パブリック関数実装 --- */

esp_err_t soil_gatt_service_init(const soil_gatt_callbacks_t *callbacks)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "GATT service already initialized");
        return ESP_OK;
    }

    if (callbacks == NULL) {
        ESP_LOGE(TAG, "Callbacks is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // コールバック設定をコピー
    memcpy(&s_callbacks, callbacks, sizeof(soil_gatt_callbacks_t));
    
    // ハンドル配列初期化
    memset(s_char_handles, 0, sizeof(s_char_handles));
    
    s_initialized = true;
    ESP_LOGI(TAG, "Soil GATT service initialized");
    return ESP_OK;
}

esp_err_t soil_gatt_service_register(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "GATT service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // GATT サービス登録
    int rc = ble_gatts_count_cfg(s_gatt_service_def);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_service_def);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GATT service registered successfully");
    ESP_LOGI(TAG, "Sensor data handle: %d", s_char_handles[SOIL_CHAR_SENSOR_DATA]);
    ESP_LOGI(TAG, "Data status handle: %d", s_char_handles[SOIL_CHAR_DATA_STATUS]);
    
    return ESP_OK;
}

esp_err_t soil_gatt_service_notify_sensor_data(const soil_ble_data_t *data)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "GATT service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        ESP_LOGE(TAG, "Data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // 接続状態確認
    uint16_t conn_handle = ble_manager_get_connection_handle();
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGD(TAG, "No active connection for notification");
        return ESP_ERR_INVALID_STATE;
    }

    // サブスクリプション状態確認
    if (!ble_manager_is_subscribed(s_char_handles[SOIL_CHAR_SENSOR_DATA])) {
        ESP_LOGD(TAG, "Client not subscribed for sensor data notifications");
        return ESP_ERR_INVALID_STATE;
    }

    // 通知送信
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, sizeof(soil_ble_data_t));
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gattc_notify_custom(conn_handle, s_char_handles[SOIL_CHAR_SENSOR_DATA], om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sensor data notification sent successfully");
    return ESP_OK;
}

uint16_t soil_gatt_service_get_char_handle(soil_characteristic_id_t char_id)
{
    if (char_id >= SOIL_CHAR_MAX) {
        return 0;
    }
    return s_char_handles[char_id];
}

bool soil_gatt_service_is_sensor_data_subscribed(void)
{
    if (!s_initialized) {
        return false;
    }
    
    return ble_manager_is_subscribed(s_char_handles[SOIL_CHAR_SENSOR_DATA]);
}

void soil_gatt_service_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    memset(&s_callbacks, 0, sizeof(s_callbacks));
    memset(s_char_handles, 0, sizeof(s_char_handles));
    s_initialized = false;
    
    ESP_LOGI(TAG, "Soil GATT service deinitialized");
}

/* --- プライベート関数実装 --- */

static int sensor_data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGD(TAG, "Sensor data access callback, op=%d", ctxt->op);

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: 
        if (s_callbacks.data_read_cb == NULL) {
            ESP_LOGE(TAG, "Data read callback not set");
            return BLE_ATT_ERR_UNLIKELY;
        }

        soil_ble_data_t sensor_data;
        esp_err_t ret = s_callbacks.data_read_cb(&sensor_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
            return BLE_ATT_ERR_UNLIKELY;
        }

        int rc = os_mbuf_append(ctxt->om, &sensor_data, sizeof(sensor_data));
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to append sensor data to mbuf");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ESP_LOGD(TAG, "Sensor data read: Temp=%.2f, Hum=%.2f, Lux=%.2f, Soil=%.2f",
                 sensor_data.temperature, sensor_data.humidity,
                 sensor_data.lux, sensor_data.soil_moisture);
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        ESP_LOGW(TAG, "Write operation not supported for sensor data");
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;

    default:
        ESP_LOGW(TAG, "Unsupported operation: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}


static int data_status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    esp_err_t ret;
    ESP_LOGD(TAG, "Data status access callback, op=%d", ctxt->op);

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: 
        if (s_callbacks.status_read_cb == NULL) {
            ESP_LOGE(TAG, "Status read callback not set");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ble_data_status_t status;
        ret = s_callbacks.status_read_cb(&status);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read status data: %s", esp_err_to_name(ret));
            return BLE_ATT_ERR_UNLIKELY;
        }

        rc = os_mbuf_append(ctxt->om, &status, sizeof(status));
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to append status data to mbuf");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ESP_LOGD(TAG, "Status data read: count=%zu, capacity=%zu, empty=%d, full=%d",
                 status.count, status.capacity, status.f_empty, status.f_full);
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: 
        if (s_callbacks.status_write_cb == NULL) {
            ESP_LOGW(TAG, "Status write callback not set");
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }

        if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(ble_data_status_t)) {
            ESP_LOGE(TAG, "Invalid write data length: %d", OS_MBUF_PKTLEN(ctxt->om));
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        ble_data_status_t write_status;
        rc = ble_hs_mbuf_to_flat(ctxt->om, &write_status, sizeof(write_status), NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to copy write data");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ret = s_callbacks.status_write_cb(&write_status);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write status data: %s", esp_err_to_name(ret));
            return BLE_ATT_ERR_UNLIKELY;
        }

        ESP_LOGD(TAG, "Status data written successfully");
        return 0;
    default:
        ESP_LOGW(TAG, "Unsupported operation: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}