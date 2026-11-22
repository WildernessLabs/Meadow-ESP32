/**
 *  @file EspEmuBtControllerStatus.c
 *
 *  Bluetooth controller status for the ESP32-S3 emulator build (esp-emu).
 *
 *  esp-emu replaces the controller's esp_bt_controller_init / _enable / _disable / _deinit with
 *  its own virtual controller, but not esp_bt_controller_get_status, which reads a variable only
 *  the real functions set.  It therefore always reports ESP_BT_CONTROLLER_STATUS_IDLE, and
 *  esp_bluedroid_init fails with "Controller not initialised" (ESP_ERR_INVALID_STATE).
 *
 *  The linker wraps (main/CMakeLists.txt, esp32s3 only) keep the status here instead: each call
 *  goes to the real (intercepted) function and, when it succeeds, moves the status on.  Calls from
 *  inside the controller library itself are not wrapped, which does not matter because esp-emu
 *  replaces those functions.
 */
#include "esp_bt.h"

static esp_bt_controller_status_t s_status = ESP_BT_CONTROLLER_STATUS_IDLE;

esp_err_t __real_esp_bt_controller_init(esp_bt_controller_config_t *cfg);
esp_err_t __real_esp_bt_controller_deinit(void);
esp_err_t __real_esp_bt_controller_enable(esp_bt_mode_t mode);
esp_err_t __real_esp_bt_controller_disable(void);

esp_err_t __wrap_esp_bt_controller_init(esp_bt_controller_config_t *cfg)
{
    const esp_err_t result = __real_esp_bt_controller_init(cfg);
    if (result == ESP_OK)
    {
        s_status = ESP_BT_CONTROLLER_STATUS_INITED;
    }
    return result;
}

esp_err_t __wrap_esp_bt_controller_deinit(void)
{
    const esp_err_t result = __real_esp_bt_controller_deinit();
    if (result == ESP_OK)
    {
        s_status = ESP_BT_CONTROLLER_STATUS_IDLE;
    }
    return result;
}

esp_err_t __wrap_esp_bt_controller_enable(esp_bt_mode_t mode)
{
    const esp_err_t result = __real_esp_bt_controller_enable(mode);
    if (result == ESP_OK)
    {
        s_status = ESP_BT_CONTROLLER_STATUS_ENABLED;
    }
    return result;
}

esp_err_t __wrap_esp_bt_controller_disable(void)
{
    const esp_err_t result = __real_esp_bt_controller_disable();
    if (result == ESP_OK)
    {
        s_status = ESP_BT_CONTROLLER_STATUS_INITED;
    }
    return result;
}

esp_bt_controller_status_t __wrap_esp_bt_controller_get_status(void)
{
    return s_status;
}
