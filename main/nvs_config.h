#pragma once

#include "esp_err.h"
#include "components/plant_logic/plant_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NVS設定管理システムを初期化
 * @return ESP_OK on success
 */
esp_err_t nvs_config_init(void);

/**
 * 植物プロファイルをNVSに保存
 * @param profile 保存する植物プロファイル
 * @return ESP_OK on success
 */
esp_err_t nvs_config_save_plant_profile(const plant_profile_t *profile);

/**
 * 植物プロファイルをNVSから読み込み
 * @param profile 読み込み先の植物プロファイル
 * @return ESP_OK on success
 */
esp_err_t nvs_config_load_plant_profile(plant_profile_t *profile);

/**
 * デフォルトの植物プロファイル設定（多肉植物向け）
 * @param profile 設定先の植物プロファイル
 */
void nvs_config_set_default_plant_profile(plant_profile_t *profile);

#ifdef __cplusplus
}
#endif