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
    
    // 高温休眠期の条件
    profile->high_temp_dormancy_max_temp = 30.0f;
    profile->high_temp_dormancy_min_temp = 25.0f;
    profile->high_temp_dormancy_min_temp_days = 4;
    
    // 低温休眠期の条件
    profile->low_temp_dormancy_min_temp = 5.0f;
    
    // 活動期の条件
    profile->active_period_min_temp = 10.0f;
    profile->active_period_max_temp = 28.0f;
    profile->active_period_consecutive_days = 3;
    
    // 土壌水分の条件
    profile->soil_dry_threshold = 2500.0f;
    profile->soil_moisture_opt_min = 1000.0f;
    profile->soil_moisture_opt_max = 1800.0f;
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
    ESP_LOGI(TAG, "High Temp Dormancy: Max %.1f°C, Min %.1f°C for %d days", 
             profile->high_temp_dormancy_max_temp,
             profile->high_temp_dormancy_min_temp,
             profile->high_temp_dormancy_min_temp_days);
    ESP_LOGI(TAG, "Low Temp Dormancy: Min %.1f°C", profile->low_temp_dormancy_min_temp);
    ESP_LOGI(TAG, "Active Period: %.1f-%.1f°C for %d consecutive days", 
             profile->active_period_min_temp,
             profile->active_period_max_temp,
             profile->active_period_consecutive_days);
    ESP_LOGI(TAG, "Soil: Dry >= %.0fmV, Optimal %.0f-%.0fmV, Watering after %d dry days",
                profile->soil_dry_threshold,
                profile->soil_moisture_opt_min,
                profile->soil_moisture_opt_max,
                profile->soil_dry_days_for_watering);
    
    nvs_close(nvs_handle);
    return ESP_OK;
}