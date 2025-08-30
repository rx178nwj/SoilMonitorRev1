#include "plant_manager.h"
#include "../../nvs_config.h"
#include "data_buffer.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <time.h>
#include "../../common_types.h"

static const char *TAG = "PlantManager";

// プライベート変数
static plant_profile_t g_plant_profile;
static bool g_initialized = false;

// プライベート関数の宣言
static plant_growth_phase_t determine_growth_phase(const plant_profile_t *profile);
static soil_condition_t determine_soil_condition(const plant_profile_t *profile);

/**
 * 植物管理システムを初期化
 */
esp_err_t plant_manager_init(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing plant management system");
    
    // NVS設定システムを初期化
    //ret = nvs_config_init();
    //if (ret != ESP_OK) {
    //    ESP_LOGE(TAG, "Failed to initialize NVS config");
    //    return ret;
    //}
    
    // データバッファシステムを初期化
    ret = data_buffer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize data buffer");
        return ret;
    }
    
    // 植物プロファイルを読み込み
    ret = nvs_config_load_plant_profile(&g_plant_profile);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load plant profile");
        return ret;
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "Plant management system initialized successfully");
    ESP_LOGI(TAG, "Plant: %s", g_plant_profile.plant_name);
    
    return ESP_OK;
}

/**
 * センサーデータを処理（データバッファに保存）
 */
void plant_manager_process_sensor_data(const soil_data_t *sensor_data) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Plant manager not initialized");
        return;
    }
    
    if (sensor_data == NULL) {
        ESP_LOGE(TAG, "Sensor data is NULL");
        return;
    }
    
    // データバッファにセンサーデータを追加
    esp_err_t ret = data_buffer_add_minute_data(sensor_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add sensor data to buffer: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "Sensor data added to buffer successfully");
    }
}

/**
 * 植物の状態を総合的に判断（データバッファの過去データを使用）
 */
plant_status_result_t plant_manager_determine_status(void) {
    plant_status_result_t result = {0};
    
    if (!g_initialized) {
        ESP_LOGE(TAG, "Plant manager not initialized");
        result.growth_phase = UNKNOWN;
        result.soil_condition = SOIL_DRY;
        return result;
    }
    
    result.growth_phase = determine_growth_phase(&g_plant_profile);
    result.soil_condition = determine_soil_condition(&g_plant_profile);
    
    return result;
}

/**
 * 生育期の文字列表現を取得
 */
const char* plant_manager_get_growth_phase_string(plant_growth_phase_t phase) {
    switch (phase) {
        case HIGH_TEMP_DORMANCY: return "高温休眠期";
        case LOW_TEMP_DORMANCY:  return "低温休眠期";
        case ACTIVE_PERIOD:      return "活動期";
        default:                 return "不明";
    }
}

/**
 * 土壌状態の文字列表現を取得
 */
const char* plant_manager_get_soil_condition_string(soil_condition_t condition) {
    switch (condition) {
        case SOIL_DRY:              return "乾燥";
        case SOIL_MOISTURE_OPTIMAL: return "適正";
        case SOIL_WET:              return "過湿";
        case NEEDS_WATERING:        return "水やりが必要";
        case ERROR_SOIL_CONDITION:  return "エラー";
        default:                    return "不明";
    }
}

/**
 * 現在の植物プロファイルを取得
 */
const plant_profile_t* plant_manager_get_profile(void) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Plant manager not initialized");
        return NULL;
    }
    return &g_plant_profile;
}

/**
 * システム全体の状態情報をログ出力
 */
void plant_manager_print_system_status(void) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Plant manager not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "=== Plant Management System Status ===");
    ESP_LOGI(TAG, "Plant: %s", g_plant_profile.plant_name);
    
    // データバッファの状態を出力
    data_buffer_print_status();
    
    // 最新のセンサーデータを表示
    minute_data_t latest_data;
    if (data_buffer_get_latest_minute_data(&latest_data) == ESP_OK) {
        ESP_LOGI(TAG, "Latest sensor data: temp=%.1f°C, humidity=%.1f%%, soil=%.0fmV",
                 latest_data.temperature, latest_data.humidity, latest_data.soil_moisture);
    }
    
    // 最新の日別サマリーを表示
    daily_summary_data_t latest_summary;
    if (data_buffer_get_latest_daily_summary(&latest_summary) == ESP_OK) {
        ESP_LOGI(TAG, "Latest daily summary: temp=%.1f-%.1f°C, soil=%.0fmV",
                 latest_summary.min_temperature, latest_summary.max_temperature, 
                 latest_summary.avg_soil_moisture);
    }
}

// プライベート関数の実装

/**
 * 生育期を判断（データバッファの過去7日データを使用）
 */
static plant_growth_phase_t determine_growth_phase(const plant_profile_t *profile) {
    daily_summary_data_t daily_summaries[7];
    uint8_t summary_count = 0;
    
    // 過去7日間の日別サマリーデータを取得
    esp_err_t ret = data_buffer_get_recent_daily_summaries(7, daily_summaries, &summary_count);
    if (ret != ESP_OK || summary_count == 0) {
        ESP_LOGW(TAG, "No sufficient daily data for growth phase determination");
        return UNKNOWN;
    }
    
    int high_temp_days = 0;
    int active_consecutive_days = 0;
    int consecutive_count = 0;
    
    // 過去のデータをチェック
    for (int i = 0; i < summary_count; i++) {
        daily_summary_data_t *day = &daily_summaries[i];
        
        // 高温休眠期の条件チェック
        if (day->max_temperature >= profile->high_temp_dormancy_max_temp) {
            ESP_LOGD(TAG, "High temp dormancy triggered: max_temp=%.1f >= %.1f", 
                     day->max_temperature, profile->high_temp_dormancy_max_temp);
            return HIGH_TEMP_DORMANCY; // 1日でも条件を満たせば高温休眠期
        }
        if (day->min_temperature >= profile->high_temp_dormancy_min_temp) {
            high_temp_days++;
        }
        
        // 低温休眠期の条件チェック
        if (day->min_temperature <= profile->low_temp_dormancy_min_temp) {
            ESP_LOGD(TAG, "Low temp dormancy triggered: min_temp=%.1f <= %.1f", 
                     day->min_temperature, profile->low_temp_dormancy_min_temp);
            return LOW_TEMP_DORMANCY; // 1日でも条件を満たせば低温休眠期
        }
        
        // 活動期の条件チェック（連続日数）
        if (day->min_temperature >= profile->active_period_min_temp && 
            day->max_temperature <= profile->active_period_max_temp) {
            consecutive_count++;
            if (consecutive_count > active_consecutive_days) {
                active_consecutive_days = consecutive_count;
            }
        } else {
            consecutive_count = 0; // 連続が途切れた場合リセット
        }
    }
    
    // 高温休眠期の日数条件チェック
    if (high_temp_days >= profile->high_temp_dormancy_min_temp_days) {
        ESP_LOGD(TAG, "High temp dormancy by days: %d >= %d", 
                 high_temp_days, profile->high_temp_dormancy_min_temp_days);
        return HIGH_TEMP_DORMANCY;
    }
    
    // 活動期の連続日数条件チェック
    if (active_consecutive_days >= profile->active_period_consecutive_days) {
        ESP_LOGD(TAG, "Active period: consecutive_days=%d >= %d", 
                 active_consecutive_days, profile->active_period_consecutive_days);
        return ACTIVE_PERIOD;
    }
    
    ESP_LOGD(TAG, "Growth phase: UNKNOWN (insufficient data or conditions not met)");
    return UNKNOWN;
}

/**
 * 土壌水分状態を判断（現在の状態のみを使用）
 */
static soil_condition_t current_soil_condition(const plant_profile_t *profile)
{
    minute_data_t latest_data;
    esp_err_t ret = data_buffer_get_latest_minute_data(&latest_data);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No latest sensor data for soil condition determination");
        return ERROR_SOIL_CONDITION;
    }
    
    ESP_LOGI(TAG, "Determining soil condition based on latest average soil moisture");
    ESP_LOGI(TAG, "Latest avg soil moisture: %.0fmV", latest_data.soil_moisture);
    ESP_LOGI(TAG, "soil condition threshold: dry AD > %.0f, optimal %.0f < AD < %.0f, we %.0f > AD", profile->soil_dry_threshold, profile->soil_moisture_opt_min, profile->soil_moisture_opt_max, profile->soil_moisture_opt_max);

    float soil_moisture = latest_data.soil_moisture;
    
    if (soil_moisture >= profile->soil_dry_threshold) {
        ESP_LOGD(TAG, "Soil dry: %.0f >= %.0f", soil_moisture, profile->soil_dry_threshold);
        return SOIL_DRY;
    } else if (soil_moisture >= profile->soil_moisture_opt_min && 
               soil_moisture <= profile->soil_moisture_opt_max) {
        ESP_LOGD(TAG, "Soil optimal: %.0f in range %.0f-%.0f", 
                 soil_moisture, profile->soil_moisture_opt_min, profile->soil_moisture_opt_max);
        return SOIL_MOISTURE_OPTIMAL;
    } else {
        ESP_LOGD(TAG, "Soil wet: %.0f < %.0f", soil_moisture, profile->soil_moisture_opt_min);
        return SOIL_WET;
    }

    return ERROR_SOIL_CONDITION; // デフォルト（到達しないはず）
}

/**
 * 土壌水分状態を判断（データバッファの過去データを使用）
 */
static soil_condition_t determine_soil_condition(const plant_profile_t *profile) {
    daily_summary_data_t daily_summaries[7];
    uint8_t summary_count = 0;
    
    // 過去7日間の日別サマリーデータを取得
    esp_err_t ret = data_buffer_get_recent_daily_summaries(7, daily_summaries, &summary_count);
    if (ret != ESP_OK || summary_count == 0) {
        ESP_LOGW(TAG, "No sufficient daily data for soil condition determination");

        return current_soil_condition(profile);
    }
    
    int consecutive_dry_days = 0;
    
    // 過去データから連続乾燥日数をカウント（最新から過去へ）
    for (int i = summary_count - 1; i >= 0; i--) {
        if (daily_summaries[i].avg_soil_moisture >= profile->soil_dry_threshold) {
            consecutive_dry_days++;
        } else {
            break; // 連続が途切れた場合終了
        }
    }
    
    // 水やりが必要かチェック
    if (consecutive_dry_days >= profile->soil_dry_days_for_watering) {
        ESP_LOGD(TAG, "Needs watering: consecutive_dry_days=%d >= %d", 
                 consecutive_dry_days, profile->soil_dry_days_for_watering);
        return NEEDS_WATERING;
    }

    // それ以外は
    // 最新の土壌水分値で判断
    return current_soil_condition(profile);
}