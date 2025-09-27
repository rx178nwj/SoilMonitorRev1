#include "nvs_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_Config";

// NVSキー定義
#define NVS_NAMESPACE "plant_config"
#define NVS_KEY_PROFILE "profile"

/**
 * デフォルトの植物プロファイル設定（多肉植物向け）
 */
void nvs_config_set_default_plant_profile(plant_profile_t *profile) {
    if (profile == NULL) {
        ESP_LOGE(TAG, "Profile pointer is NULL");
        return;
    }

    // 植物名
    strcpy(profile->plant_name, "Succulent Plant");

    // 土壌水分の条件
    profile->soil_dry_threshold = 2500.0f;
    profile->soil_wet_threshold = 1000.0f;
    profile->soil_dry_days_for_watering = 3;

    ESP_LOGI(TAG, "Default plant profile set for: %s", profile->plant_name);
}

/**
 * 植物プロファイルをNVSに保存
 */
esp_err_t nvs_config_save_plant_profile(const plant_profile_t *profile) {
    if (profile == NULL) {
        ESP_LOGE(TAG, "Profile pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // NVSハンドルを開く
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // プロファイルをblobとして保存
    err = nvs_set_blob(nvs_handle, NVS_KEY_PROFILE, profile, sizeof(plant_profile_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving plant profile: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // 変更をコミット
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Plant profile saved successfully: %s", profile->plant_name);
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * 植物プロファイルをNVSから読み込み
 */
esp_err_t nvs_config_load_plant_profile(plant_profile_t *profile) {
    if (profile == NULL) {
        ESP_LOGE(TAG, "Profile pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size = sizeof(plant_profile_t);

    // NVSハンドルを開く（読み取り専用）
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // NVSパーティション自体が見つからない場合
        ESP_LOGW(TAG, "NVS partition not found, creating with default profile");
        nvs_config_set_default_plant_profile(profile);

        // デフォルト値をNVSに保存（書き込みモードで再試行）
        esp_err_t save_err = nvs_config_save_plant_profile(profile);
        if (save_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save default profile, continuing with defaults");
        }
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        // 致命的でない場合はデフォルト値で続行
        ESP_LOGW(TAG, "Using default profile due to NVS error");
        nvs_config_set_default_plant_profile(profile);
        return ESP_OK;
    }

    // プロファイルをblobとして読み込み
    err = nvs_get_blob(nvs_handle, NVS_KEY_PROFILE, profile, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Plant profile not found in NVS, using default values");
        nvs_close(nvs_handle);

        // デフォルト値を設定
        nvs_config_set_default_plant_profile(profile);

        // デフォルト値をNVSに保存
        esp_err_t save_err = nvs_config_save_plant_profile(profile);
        if (save_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save default profile to NVS: %s", esp_err_to_name(save_err));
        }

        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading plant profile: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);

        // エラーの場合もデフォルト値で続行
        ESP_LOGW(TAG, "Using default profile due to read error");
        nvs_config_set_default_plant_profile(profile);
        return ESP_OK;
    }

    // サイズ検証
    if (required_size != sizeof(plant_profile_t)) {
        ESP_LOGE(TAG, "Profile size mismatch. Expected: %d, Got: %d",
                 sizeof(plant_profile_t), required_size);
        nvs_close(nvs_handle);

        // サイズ不整合の場合もデフォルト値で続行
        ESP_LOGW(TAG, "Using default profile due to size mismatch");
        nvs_config_set_default_plant_profile(profile);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Plant profile loaded successfully: %s", profile->plant_name);
    ESP_LOGI(TAG, "Soil: Dry >= %.0fmV, Wet <= %.0fmV, Watering after %d dry days",
                profile->soil_dry_threshold,
                profile->soil_wet_threshold,
                profile->soil_dry_days_for_watering);

    nvs_close(nvs_handle);
    return ESP_OK;
}