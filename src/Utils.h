#ifndef _UTILS_H_
#define _UTILS_H_

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

#endif
