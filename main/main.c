#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "OLED.h"
#include "WIFI.h"
#include "ntp_time.h"
#include "EXIT_KEY.h"
#include "XL9555.h"
#include "MAX30102.h"
#include "MPU6050.h"
#include "NTC.h"
#include "ble.h"

// ==================== 配置宏 ====================
#define SENSOR_READ_MS          50
#define OLED_REFRESH_MS         1000
#define NTP_RETRY_COUNT         30
#define NTP_TIMEOUT_MS          1000
#define WARMUP_SAMPLES          60

#define DATA_AGING_TIMEOUT_MS   10000
#define WRIST_CHECK_INTERVAL    10

#define TASK_PRIO_SENSOR        10
#define TASK_PRIO_MPU6050       8
#define TASK_PRIO_DISPLAY       5
#define TASK_PRIO_TEMP          7
#define TASK_PRIO_BLE           6

#define TASK_STACK_SENSOR       8192
#define TASK_STACK_MPU6050      4096
#define TASK_STACK_DISPLAY      8192
#define TASK_STACK_TEMP         4096
#define TASK_STACK_BLE          4096

#define HR_MIN_VALID            40
#define HR_MAX_VALID            180
#define SPO2_MIN_VALID          85
#define SPO2_MAX_VALID          100

#define DEBUG_PRINT_ENABLE      0
#define TEMP_DEBUG_ENABLE       1

#if DEBUG_PRINT_ENABLE
    #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

#if TEMP_DEBUG_ENABLE
    #define TEMP_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define TEMP_PRINT(fmt, ...) ((void)0)
#endif

// ==================== 全局外部声明 ====================
extern uint32_t mpu6050_step_count;

// ==================== 数据结构 ====================
typedef struct {
    uint8_t heart_rate;
    uint8_t spo2;
    bool wrist_detected;
    uint32_t ir_value;
    uint32_t red_value;
    bool sensor_ready;
    uint32_t sample_count;

    uint8_t cached_hr;
    uint8_t cached_spo2;
    TickType_t cache_timestamp;
} SensorData_t;

// ==================== 全局变量 ====================
static SensorData_t s_sensor = {0};
static EventGroupHandle_t s_event_group = NULL;

static ntc_config_t ntc_config;
static float g_temperature = 0.0f;

#define EVENT_TIME_SYNCED       (1 << 0)

// ==================== NTP辅助函数 ====================
static void wait_ntp_sync(void)
{
    int retry = 0;

    DEBUG_PRINT("[INFO] Waiting for WiFi...\n");
    while (!wifi_is_connected() && retry < NTP_RETRY_COUNT) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }

    if (!wifi_is_connected()) {
        DEBUG_PRINT("[WARN] WiFi not connected, using default time\n");
        return;
    }

    DEBUG_PRINT("[INFO] WiFi connected, syncing NTP...\n");
    retry = 0;
    time_t now;
    while ((now = time(NULL)) < 1000000000 && retry < NTP_RETRY_COUNT) {
        vTaskDelay(pdMS_TO_TICKS(NTP_TIMEOUT_MS));
        retry++;
    }

    if (now >= 1000000000) {
        DEBUG_PRINT("[INFO] NTP sync success\n");
        xEventGroupSetBits(s_event_group, EVENT_TIME_SYNCED);
    } else {
        DEBUG_PRINT("[WARN] NTP sync failed, using default time\n");
    }
}

// ==================== 星期映射 ====================
static const uint8_t WEEK_MAP[] = {34, 28, 29, 30, 31, 32, 33};

static inline uint8_t get_weekday_index(int wday)
{
    return (wday >= 0 && wday <= 6) ? WEEK_MAP[wday] : WEEK_MAP[1];
}

// ==================== 数据老化核心逻辑 ====================
static inline void update_data_cache(uint8_t hr, uint8_t spo2)
{
    if (hr > 0 && spo2 > 0) {
        s_sensor.cached_hr = hr;
        s_sensor.cached_spo2 = spo2;
        s_sensor.cache_timestamp = xTaskGetTickCount();
    }
}

static inline bool is_cache_expired(void)
{
    if (s_sensor.cache_timestamp == 0) {
        return true;
    }

    TickType_t elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - s_sensor.cache_timestamp);
    return (elapsed_ms >= DATA_AGING_TIMEOUT_MS);
}

static inline uint8_t get_display_hr(void)
{
    if (s_sensor.wrist_detected && s_sensor.sensor_ready && s_sensor.heart_rate > 0) {
        return s_sensor.heart_rate;
    }
    if (s_sensor.wrist_detected && !is_cache_expired() && s_sensor.cached_hr > 0) {
        return s_sensor.cached_hr;
    }
    return 0;
}

static inline uint8_t get_display_spo2(void)
{
    if (s_sensor.wrist_detected && s_sensor.sensor_ready && s_sensor.spo2 > 0) {
        return s_sensor.spo2;
    }
    if (s_sensor.wrist_detected && !is_cache_expired() && s_sensor.cached_spo2 > 0) {
        return s_sensor.cached_spo2;
    }
    return 0;
}

static const char* get_display_status(void)
{
    if (!s_sensor.wrist_detected) return "Pls wear";
    if (!s_sensor.sensor_ready) return "Warming";
    if (is_cache_expired() && s_sensor.cached_hr == 0) return "No data";
    return NULL;
}

// ==================== 传感器数据更新 ====================
static inline bool is_heart_rate_valid(uint8_t hr)
{
    return hr >= HR_MIN_VALID && hr <= HR_MAX_VALID;
}

static inline bool is_spo2_valid(uint8_t spo2)
{
    return spo2 >= SPO2_MIN_VALID && spo2 <= SPO2_MAX_VALID;
}

static void update_sensor_data(void)
{
    if (!s_sensor.wrist_detected || !s_sensor.sensor_ready) {
        s_sensor.heart_rate = 0;
        s_sensor.spo2 = 0;
        return;
    }

    uint8_t hr_tmp = max30102_get_heart_rate();
    uint8_t spo2_tmp = max30102_get_spo2();
    bool has_valid_data = false;

    if (is_heart_rate_valid(hr_tmp)) {
        s_sensor.heart_rate = hr_tmp;
        has_valid_data = true;
    }
    if (is_spo2_valid(spo2_tmp)) {
        s_sensor.spo2 = spo2_tmp;
        has_valid_data = true;
    }

    if (has_valid_data && s_sensor.heart_rate > 0 && s_sensor.spo2 > 0) {
        update_data_cache(s_sensor.heart_rate, s_sensor.spo2);
    }
}

static void print_sensor_status(void)
{
#if DEBUG_PRINT_ENABLE
    const char* status = get_display_status();
    if (status) {
        DEBUG_PRINT("[SENSOR] %s | IR=%lu | RED=%lu\n", status, (unsigned long)s_sensor.ir_value, (unsigned long)s_sensor.red_value);
    } else {
        DEBUG_PRINT("[SENSOR] ✅ HR=%d | SpO2=%d%% | IR=%lu | RED=%lu\n",
               get_display_hr(), get_display_spo2(),
               (unsigned long)s_sensor.ir_value, (unsigned long)s_sensor.red_value);
    }
#endif
}

// ==================== OLED显示函数 ====================
static void oled_show_alarm(void)
{
    OLED_Clear();
    const uint8_t x = 24;
    const uint8_t page = 2;
    const uint8_t alarm_chars[] = {15, 16, 17, 18, 21};
    for (int i = 0; i < 5; i++) {
        OLED_ShowChinese(x + i * 16, page, alarm_chars[i]);
    }
    OLED_Refresh();
}

static void oled_show_heart_rate(uint8_t x, uint8_t page)
{
    OLED_ShowChinese(x, page, 0);
    OLED_ShowChinese(x + 16, page, 1);
    OLED_ShowChinese(x + 32, page, 22);

    uint8_t hr = get_display_hr();
    if (hr > 0) {
        OLED_ShowNumAutoSimple(x + 58, page, hr);
    } else {
        OLED_ShowString(x + 58, page, "---");
    }

    OLED_ShowChinese(x + 63, page, 40);
    OLED_ShowNum8x16(x + 77, page, 11);
    OLED_ShowChinese(x + 91, page, 41);
}

static void oled_show_spo2(uint8_t x, uint8_t page)
{
    const uint8_t spo2_chars[] = {2, 3, 4, 5, 6};
    for (int i = 0; i < 5; i++) {
        OLED_ShowChinese(x + i * 16, page, spo2_chars[i]);
    }
    OLED_ShowChinese(x + 80, page, 22);

    uint8_t spo2 = get_display_spo2();
    if (spo2 > 0) {
        OLED_ShowNumAutoSimple(x + 104, page, spo2);
    } else {
        OLED_ShowString(x + 104, page, "---");
    }
    OLED_ShowNum8x16(x + 109, page, 12);
}

static void oled_show_step_count(uint8_t x, uint8_t page)
{
    OLED_ShowChinese(x, page, 35);
    OLED_ShowChinese(x + 16, page, 36);
    OLED_ShowChinese(x + 32, page, 22);
    OLED_ShowNumAutoSimple(x + 78, page, mpu6050_step_count);
}

static void oled_show_Temperature(uint8_t x, uint8_t page)
{
    OLED_ShowChinese(x, page, 7);
    OLED_ShowChinese(x + 16, page, 8);
    OLED_ShowChinese(x + 32, page, 22);

    if (g_temperature > 0.0f && g_temperature < 100.0f) {
        int temp_round = (int)(g_temperature + 0.5f);
        OLED_ShowNumAutoSimple(x + 58, page, temp_round);
    } else {
        OLED_ShowString(x + 58, page, "--");
    }

    OLED_ShowChinese(x + 74, page, 42);
}

static void oled_show_test_mode(void)
{
    OLED_Clear();
    oled_show_Temperature(2, 4);
    oled_show_heart_rate(2, 0);
    oled_show_spo2(2, 2);

    const char* status = get_display_status();
    if (status) {
        OLED_ShowString(20, 6, status);
    }
    OLED_Refresh();
}

static void oled_show_normal_mode(void)
{
    OLED_Clear();
    struct tm tm;
    ntp_get_time(&tm);

    OLED_ShowTime8x16(2, 0, tm.tm_hour, tm.tm_min);

    int year = tm.tm_year + 1900;
    for (int i = 0; i < 4; i++) {
        uint8_t digit = (uint8_t)((year / (int)pow(10, 3 - i)) % 10);
        OLED_ShowNum8x16(2 + i * 8, 2, digit);
    }
    OLED_ShowChinese(34, 2, 23);

    int month = tm.tm_mon + 1;
    OLED_ShowNumAutoSimple(55, 2, month);
    OLED_ShowChinese(60, 2, 24);

    OLED_ShowNumAutoSimple(85, 2, tm.tm_mday);
    OLED_ShowChinese(90, 2, 25);

    OLED_ShowChinese(2, 4, 26);
    OLED_ShowChinese(18, 4, 27);
    OLED_ShowChinese(34, 4, get_weekday_index(tm.tm_wday));

    oled_show_step_count(2, 6);
    OLED_Refresh();
}

// ==================== MAX30102 任务 ====================
void max30102_task(void *arg)
{
    esp_err_t ret;
    for (int attempt = 0; attempt < 5; attempt++) {
        ret = max30102_init();
        if (ret == ESP_OK) break;
        DEBUG_PRINT("[ERROR] MAX30102 init attempt %d failed\n", attempt + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (ret != ESP_OK) {
        DEBUG_PRINT("[ERROR] MAX30102 init failed!\n");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    DEBUG_PRINT("[INFO] MAX30102 init success\n");
    vTaskDelay(pdMS_TO_TICKS(300));

    uint32_t error_count = 0;
    const uint32_t WARMUP_HALF = WARMUP_SAMPLES / 2;

    while (1) {
        uint32_t ir = 0, red = 0;
        ret = max30102_read_fifo(&ir, &red);

        if (ret == ESP_OK) {
            s_sensor.sample_count++;

            if (s_sensor.sample_count < WARMUP_SAMPLES) {
                s_sensor.sensor_ready = false;
                if (s_sensor.sample_count == WARMUP_HALF) {
                    DEBUG_PRINT("[INFO] Warming up... %lu/%d\n", (unsigned long)s_sensor.sample_count, WARMUP_SAMPLES);
                }
            } else if (!s_sensor.sensor_ready) {
                s_sensor.sensor_ready = true;
                DEBUG_PRINT("[INFO] Sensor ready!\n");
            }

            if ((s_sensor.sample_count % WRIST_CHECK_INTERVAL) == 0) {
                s_sensor.wrist_detected = max30102_is_wrist_detected();
            }

            s_sensor.ir_value = ir;
            s_sensor.red_value = red;
            update_sensor_data();
            print_sensor_status();
            error_count = 0;
        } else {
            error_count++;
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_MS));
    }
}

// ==================== MPU6050 任务 ====================
void mpu6050_task_wrapper(void *arg)
{
    mpu6050_task(arg);
}

// ==================== NTC 体温任务 ====================
void ntc_temp_task(void *arg)
{
    ntc_config = ntc_get_default_config();
    ntc_init(&ntc_config);
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        g_temperature = ntc_read_temperature_avg(&ntc_config, 20);
        int temp_integer = (int)(g_temperature + 0.5f);
        TEMP_PRINT("[TEMP DEBUG] 实时体温：%.2f ℃  |  四舍五入整数：%d ℃\n", g_temperature, temp_integer);
        DEBUG_PRINT("[NTC] 体温: %d℃\n", temp_integer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ==================== 显示任务 ====================
void display_task(void *arg)
{
    OLED_Init();
    OLED_Clear();
    OLED_Refresh();
    exit_init(XL9555_I2C_NUM, XL9555_SDA_PIN, XL9555_SCL_PIN);

    wait_ntp_sync();
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        exit_check_key_and_switch_mode();

        if (oled_show_alarm_flag) {
            oled_show_alarm();
        } else if (oled_display_mode == OLED_MODE_NORMAL) {
            oled_show_normal_mode();
        } else {
            oled_show_test_mode();
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(OLED_REFRESH_MS));
    }
}

// ==================== BLE 发送健康数据任务 ====================
// ==================== BLE 发送原始十进制数据（二进制格式）====================
void ble_send_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    ble_init();

    // 定义一包数据结构：纯数字，手机直接按字节解析即可
    typedef struct {
        uint8_t hr;        // 心率
        uint8_t spo2;      // 血氧
        int8_t  temp;      // 体温（整数）
        uint16_t step;     // 步数
    } __attribute__((packed)) ble_data_t;

    while (1) {
        if (ble_is_connected()) {
            ble_data_t pkt = {0};

            pkt.hr   = get_display_hr();
            pkt.spo2 = get_display_spo2();
            pkt.temp = (int8_t)(g_temperature + 0.5f);
            pkt.step = (uint16_t)mpu6050_step_count;

            // 直接发原始数字，不再发字符串
            ble_send_data((uint8_t*)&pkt, sizeof(pkt));

            DEBUG_PRINT("[BLE] 原始数字: HR=%d SpO2=%d Temp=%d Step=%d\n",
                pkt.hr, pkt.spo2, pkt.temp, pkt.step);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ==================== 系统初始化 ====================
static esp_err_t system_init(void)
{
    s_event_group = xEventGroupCreate();
    if (!s_event_group) return ESP_FAIL;
    memset(&s_sensor, 0, sizeof(s_sensor));
    wifi_init_sta();
    ntp_init();
    return ESP_OK;
}

// ==================== 创建任务 ====================
static void create_tasks(void)
{
    const struct {
        TaskFunction_t func;
        const char *name;
        uint32_t stack;
        UBaseType_t priority;
        BaseType_t core;
    } tasks[] = {
        {max30102_task,       "max30102_task",   TASK_STACK_SENSOR,   TASK_PRIO_SENSOR,   0},
        {mpu6050_task_wrapper,"mpu6050_task",    TASK_STACK_MPU6050,  TASK_PRIO_MPU6050,  tskNO_AFFINITY},
        {ntc_temp_task,       "ntc_temp_task",   TASK_STACK_TEMP,     TASK_PRIO_TEMP,     tskNO_AFFINITY},
        {display_task,        "display_task",    TASK_STACK_DISPLAY,  TASK_PRIO_DISPLAY,  1},
        {ble_send_task,       "ble_send_task",   TASK_STACK_BLE,      TASK_PRIO_BLE,      tskNO_AFFINITY},
    };

    for (int i = 0; i < sizeof(tasks)/sizeof(tasks[0]); i++) {
        if (tasks[i].core == tskNO_AFFINITY) {
            xTaskCreate(tasks[i].func, tasks[i].name, tasks[i].stack, NULL, tasks[i].priority, NULL);
        } else {
            xTaskCreatePinnedToCore(tasks[i].func, tasks[i].name, tasks[i].stack, NULL, tasks[i].priority, NULL, tasks[i].core);
        }
    }
}

// ==================== 主函数 ====================
void app_main(void)
{
    DEBUG_PRINT("\n========================================\n");
    DEBUG_PRINT("       智能手环 - 心率/血氧/体温/BLE\n");
    DEBUG_PRINT("========================================\n\n");

    system_init();
    create_tasks();
    vTaskDelete(NULL);
}