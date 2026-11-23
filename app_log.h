/**
 * @file     app_log.h
 * @brief    应用日志缓冲组件 —— 拦截 ESP_LOG 输出入环形缓冲，支持 NVS 持久化（C++ class）
 *
 * 功能：接管 esp_log 的 vprintf 输出，日志同时进串口与内部环形缓冲，
 *       可通过 HTTP 页面回看，并支持断电前保存 / 上电恢复（NVS）。
 * 修改：2026-11-21 由 app_log.c 重构为 C++ class
 */
#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* 便捷日志宏（透传 ESP_LOG） */
#define APP_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define APP_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define APP_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define APP_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define APP_LOGV(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

class AppLog {
public:
    static constexpr int BUFFER_SIZE        = 4096; /* 日志环形缓冲大小（字节） */
    static constexpr int LINE_MAX           = 128;  /* 单行日志最大长度 */
    static constexpr int AUTO_SAVE_INTERVAL = 10;   /* 累计多少条未保存日志后自动存 NVS */

    /* 全局唯一实例 */
    static AppLog &instance();

    /* 初始化：创建互斥锁、接管日志输出、从 NVS 恢复、开启自动保存 */
    void begin();

    /* 读取日志缓冲内容 / 累计日志条数 */
    const char *buffer() const { return m_buffer; }
    int count() const { return m_count; }

    /* NVS 持久化：保存 / 恢复 */
    void saveToNvs();
    void loadFromNvs();

    /* 自动保存开关 */
    void setAutoSave(bool enable) { m_autoSave = enable; }

private:
    AppLog() = default;

    static int outputThunk(const char *fmt, va_list args);
    int outputImpl(const char *fmt, va_list args);

    char m_buffer[BUFFER_SIZE] = {0};
    int  m_offset   = 0;
    int  m_count    = 0;
    int  m_unsaved  = 0;
    bool m_autoSave = false;

    SemaphoreHandle_t m_mutex           = nullptr;
    vprintf_like_t    m_originalVprintf = nullptr;
};

#endif /* APP_LOG_H */
