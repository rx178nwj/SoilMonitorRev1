#pragma once

#include <time.h>
#include "esp_err.h"
#include "../../common_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * 設定値管理構造体
 */
typedef struct {
    char plant_name[32];                    // 植物の名前
    // 高温休眠期 の条件
    float high_temp_dormancy_max_temp;      // この温度以上で高温休眠期 (例: 30.0℃)
    float high_temp_dormancy_min_temp;      // この最低温度以上の日が続くと高温休眠期 (例: 25.0℃)
    int high_temp_dormancy_min_temp_days;   // ↑上記の条件を判定する日数 (例: 4日/週)
    // 低温休眠期 の条件
    float low_temp_dormancy_min_temp;       // この温度以下で低温休眠期 (例: 5.0℃)
    // 活動期 の条件
    float active_period_min_temp;           // この最低温度以上 (例: 10.0℃)
    float active_period_max_temp;           // この最高温度以下 (例: 28.0℃)
    int active_period_consecutive_days;     // ↑の条件が連続する日数 (例: 3日)
    // 土壌水分 の条件
    float soil_dry_threshold;               // このmV以上で「乾燥」 (例: 2500.0mV)
    float soil_moisture_opt_min;            // このmV以上で「適正」 (例: 1000.0mV)
    float soil_moisture_opt_max;            // このmV以下で「適正」 (例: 1800.0mV)
    int soil_dry_days_for_watering;         // この日数以上乾燥が続いたら水やりを指示 (例: 3日)
} plant_profile_t;

/**
 * 植物の状態を示す列挙型
 */
typedef enum {
    UNKNOWN,
    HIGH_TEMP_DORMANCY,  // 高温休眠期
    LOW_TEMP_DORMANCY,   // 低温休眠期
    ACTIVE_PERIOD,       // 活動期
} plant_growth_phase_t;

typedef enum {
    SOIL_DRY,              // 乾燥
    SOIL_MOISTURE_OPTIMAL, // 適正
    SOIL_WET,              // 過湿
    NEEDS_WATERING,        // 水やりが必要
    ERROR_SOIL_CONDITION   // エラー
} soil_condition_t;

/**
 * 植物管理システムの結果構造体
 */
typedef struct {
    plant_growth_phase_t growth_phase;
    soil_condition_t soil_condition;
} plant_status_result_t;

// 公開関数

/**
 * 植物管理システムを初期化
 * @return ESP_OK on success
 */
esp_err_t plant_manager_init(void);

/**
 * センサーデータを処理（データバッファに保存し、判断ロジック用データも更新）
 * @param sensor_data センサーデータ
 */
void plant_manager_process_sensor_data(const soil_data_t *sensor_data);

/**
 * 植物の状態を総合的に判断（データバッファの過去データを使用）
 * @return 植物状態の判断結果
 */
plant_status_result_t plant_manager_determine_status(void);

/**
 * 生育期の文字列表現を取得
 * @param phase 生育期
 * @return 文字列表現
 */
const char* plant_manager_get_growth_phase_string(plant_growth_phase_t phase);

/**
 * 土壌状態の文字列表現を取得
 * @param condition 土壌状態
 * @return 文字列表現
 */
const char* plant_manager_get_soil_condition_string(soil_condition_t condition);

/**
 * 現在の植物プロファイルを取得
 * @return 植物プロファイルへのポインタ
 */
const plant_profile_t* plant_manager_get_profile(void);

/**
 * システム全体の状態情報をログ出力
 */
void plant_manager_print_system_status(void);

#ifdef __cplusplus
}
#endif