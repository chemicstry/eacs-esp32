#include "AlarmApp.h"

static const uint8_t ALARM_DETECT_PIN = CONFIG_ALARM_DETECT_PIN;
static const uint8_t ALARM_TOGGLE_PIN = CONFIG_ALARM_TOGGLE_PIN;

void AlarmApp::Start()
{
    App::Start();

    pinMode(ALARM_DETECT_PIN, INPUT);
    pinMode(ALARM_TOGGLE_PIN, OUTPUT);
    digitalWrite(ALARM_TOGGLE_PIN, LOW);

    service.RPC->bind("alarm:disarm", [this]() {
        return Disarm();
    });

    while(1) {
        Toggle();
        delay(2000);
    }
}

bool AlarmApp::IsArmed()
{
    return digitalRead(ALARM_DETECT_PIN);
}

void AlarmApp::Toggle()
{
    digitalWrite(ALARM_TOGGLE_PIN, HIGH);
    delay(500);
    digitalWrite(ALARM_TOGGLE_PIN, LOW);
}

uint32_t AlarmApp::Disarm()
{
    if (!IsArmed())
        return DISARM_RESULT_ALREADY_DISARMED;
    
    Toggle();
    delay(100);

    if (!IsArmed())
        return DISARM_RESULT_SUCCESS;
    
    return DISARM_RESULT_FAIL;
}
