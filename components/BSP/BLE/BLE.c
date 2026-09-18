#include "ble.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define TAG "BLE_DEBUG"
#define DEVICE_NAME     "ESP32_BLE"
#define UUID16_SERVICE  0xFF00
#define UUID16_CHR      0xFF01

static uint16_t s_conn_handle = 0;
static bool s_connected = false;
static uint16_t s_chr_val_handle = 0;

// 调试日志开关
#define BLE_DEBUG 1

static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = ctxt->om->om_len;
        uint8_t *data = ctxt->om->om_data;

        ESP_LOGI(TAG, "【接收数据】长度: %d", len);
        ESP_LOGI(TAG, "【接收内容】%.*s", len, data);
        ESP_LOG_BUFFER_HEX(TAG, data, len);
    }
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            s_connected = true;
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "==================================");
            ESP_LOGI(TAG, "【蓝牙已连接】handle: %d", s_conn_handle);
            ESP_LOGI(TAG, "==================================");
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            s_connected = false;
            ESP_LOGW(TAG, "==================================");
            ESP_LOGW(TAG, "【蓝牙断开连接】原因: 0x%02X", event->disconnect.reason);
            ESP_LOGW(TAG, "==================================");

            // 重启广播
            struct ble_gap_adv_params adv_params = {0};
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
            ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                              &adv_params, gap_event, NULL);
            ESP_LOGI(TAG, "【已重启广播】");
            break;
    }
    return 0;
}

static void adv_start(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                      &adv_params, gap_event, NULL);

    ESP_LOGI(TAG, "【广播启动成功】设备名: %s", DEVICE_NAME);
}

static const struct ble_gatt_chr_def s_chars[] = {
    {
        .uuid = BLE_UUID16_DECLARE(UUID16_CHR),
        .access_cb = chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &s_chr_val_handle,
    },
    {0}
};

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID16_SERVICE),
        .characteristics = s_chars,
    },
    {0}
};

static void host_task(void *param)
{
    nimble_port_run();
}

void ble_init(void)
{
    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "【蓝牙初始化开始】");
    ESP_LOGI(TAG, "==================================");

    nvs_flash_init();
    nimble_port_init();
    esp_nimble_hci_init();

    // 关闭配对 / 安全连接（免密码直连）
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_our_key_dist = 0;
    ble_hs_cfg.sm_their_key_dist = 0;

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(s_svcs);
    ble_gatts_add_svcs(s_svcs);

    nimble_port_freertos_init(host_task);
    adv_start();

    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "【蓝牙初始化完成】");
    ESP_LOGI(TAG, "【设备名称】%s", DEVICE_NAME);
    ESP_LOGI(TAG, "【服务UUID】0xFF00");
    ESP_LOGI(TAG, "【特征UUID】0xFF01");
    ESP_LOGI(TAG, "==================================");
}

// 发送数据（带调试日志）
bool ble_send_data(const uint8_t *data, uint16_t len)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "【发送失败】未连接");
        return false;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        ESP_LOGE(TAG, "【发送失败】内存不足");
        return false;
    }

    ble_gattc_notify_custom(s_conn_handle, s_chr_val_handle, om);

    ESP_LOGI(TAG, "【发送成功】长度: %d", len);
    ESP_LOG_BUFFER_HEX(TAG, data, len);
    return true;
}

bool ble_is_connected(void)
{
    return s_connected;
}