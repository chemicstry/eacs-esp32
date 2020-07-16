#include "Arduino.h"

#include "esp_log.h"
static const char* TAG = "main";

#include "Utils.h"

#if defined(CONFIG_APP_DOORS)
    #include "Apps/DoorApp.h"
    DoorApp app;
#elif defined(CONFIG_APP_ALARM)
    #include "Apps/AlarmApp.h"
    AlarmApp app;
#else
    #error "App not defined"
#endif

extern "C" void app_main() {
    initArduino();

    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // Configures task state reporting if enabled in menuconfig
    EnableTaskStats();

    ESP_LOGI(TAG, "Starting app...");

    app.Start();
}
