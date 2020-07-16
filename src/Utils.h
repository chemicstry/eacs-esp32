#pragma once

#include "ByteBuffer.h"
#include <string>
#include "esp_log.h"

inline std::string BinaryDataToHexString(const BinaryData& data)
{
    std::string str;
    str.reserve(data.size()*2+1);

    char buf[3];

    for (const uint8_t _byte : data)
    {
        sprintf(buf, "%02X", _byte);
        str += buf;
    }

    return str;
}

inline BinaryData HexStringToBinaryData(const std::string& str)
{
    BinaryData data;

    for (unsigned int i = 0; i < str.length(); i += 2) {
        std::string byteString = str.substr(i, 2);
        char _byte = (char) strtol(byteString.c_str(), NULL, 16);
        data.push_back(_byte);
    }

    return data;
}

inline void tone(int pin, int freq, int duration)
{
    int channel = 0;
    int resolution = 8;

    ledcSetup(channel, 2000, resolution);
    ledcAttachPin(pin, channel);
    ledcWriteTone(channel, freq);
    ledcWrite(channel, 255);
    delay(duration);
    ledcWrite(channel, 0);
}

inline void beep(int duration)
{
    #if CONFIG_BUZZER_PWM
        tone(CONFIG_BUZZER_PIN, 2000, duration);
    #else
        digitalWrite(CONFIG_BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(CONFIG_BUZZER_PIN, LOW);
    #endif
}

inline void beep_short() { beep(150); }
inline void beep_long() { beep(800); }

#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
inline void PrintTaskStats(void* parameter)
{
    static char taskInfo[1000];

    while (1)
    {
        // Get task status
        vTaskList(taskInfo);
        ESP_LOGI("STATS", "Task Info:\nTask            State   Prio    Stack    Num\n%s", taskInfo);

        // 5 seconds
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

inline void EnableTaskStats()
{
    xTaskCreate(PrintTaskStats, "TaskStats", 2048, NULL, 1, NULL);
}
#else
inline void EnableTaskStats()
{
    // Do nothing
}
#endif
