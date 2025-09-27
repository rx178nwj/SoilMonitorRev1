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
    float soil_dry_threshold;               // このmV以上で「乾燥」 (例: 2500.0mV)
    float soil_wet_threshold;               // このmV以下で「湿潤」 (例: 1000.0mV)
    int soil_dry_days_for_watering;         // この日数以上乾燥が続いたら水やりを指示 (例: 3日)
} plant_profile_t;

/**
 * 土壌の状態を示す列挙型
 */
typedef enum {
    SOIL_DRY,              // 乾燥
    SOIL_WET,              // 湿潤
    NEEDS_WATERING,        // 灌水要求
    WATERING_COMPLETED,    // 灌水完了
    ERROR_SOIL_CONDITION   // エラー
} soil_condition_t;

/**
 * 植物管理システムの結果構造体
 */
typedef struct {
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