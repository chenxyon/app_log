/**
 * @file     app_log.cpp
 * @brief    应用日志缓冲组件实现
 * 修改：2026-11-21 由 app_log.c 重构为 C++ class
 */
#include "app_log.h"

#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "app_log"

AppLog &AppLog::instance()
{
    static AppLog inst;
    return inst;
}

/* esp_log 全局回调：转发到单例实现 */
int AppLog::outputThunk(const char *fmt, va_list args)
{
    return AppLog::instance().outputImpl(fmt, args);
}

int AppLog::outputImpl(const char *fmt, va_list args)
{
    /* 同时输出到串口 */
    if (m_originalVprintf) {
        m_originalVprintf(fmt, args);
    }

    char line[LINE_MAX];
    int len = vsnprintf(line, LINE_MAX, fmt, args);
    if (len <= 0) return len;

    if (m_mutex) xSemaphoreTake(m_mutex, portMAX_DELAY);

    if (m_offset + len + 1 >= BUFFER_SIZE) {
        m_offset = 0;
    }
    memcpy(m_buffer + m_offset, line, len);
    m_offset += len;
    m_buffer[m_offset] = '\0';
    m_count++;
    m_unsaved++;

    if (m_autoSave && m_unsaved >= AUTO_SAVE_INTERVAL) {
        saveToNvs();
        m_unsaved = 0;
    }

    if (m_mutex) xSemaphoreGive(m_mutex);
    return len;
}

void AppLog::begin()
{
    m_mutex = xSemaphoreCreateMutex();
    memset(m_buffer, 0, BUFFER_SIZE);
    m_offset = 0;
    m_count = 0;
    m_unsaved = 0;

    /* 保存原始 vprintf，确保日志同时输出到串口 */
    m_originalVprintf = esp_log_set_vprintf((vprintf_like_t)outputThunk);

    esp_log_level_set("KEYBOARD", ESP_LOG_DEBUG);
    esp_log_level_set("BUZZER", ESP_LOG_DEBUG);
    esp_log_level_set("OLED", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    esp_log_level_set("WIFI_HTTP", ESP_LOG_DEBUG);

    loadFromNvs();
    setAutoSave(true);

    ESP_LOGI("APP_LOG", "Log component initialized, buffer %d bytes, auto-save ON", BUFFER_SIZE);
}

void AppLog::saveToNvs()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return;

    nvs_set_str(handle, "log_buf", m_buffer);
    nvs_set_i32(handle, "log_count", m_count);
    nvs_set_i32(handle, "log_offset", m_offset);
    nvs_commit(handle);
    nvs_close(handle);
}

void AppLog::loadFromNvs()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return;

    size_t required_size = BUFFER_SIZE;
    ret = nvs_get_str(handle, "log_buf", m_buffer, &required_size);
    if (ret == ESP_OK) {
        m_offset = strlen(m_buffer);
        int32_t count = 0;
        nvs_get_i32(handle, "log_count", &count);
        m_count = count;
        ESP_LOGI("APP_LOG", "Restored %d log entries from NVS", count);
    }

    nvs_close(handle);
}

#include "app_log_version.h"
#include "version_manager.h"
COMPONENT_VERSION_REGISTER(app_log, APP_LOG_VERSION_STRING)
