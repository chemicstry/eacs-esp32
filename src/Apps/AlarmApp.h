#pragma once

#include "App.h"

enum DisarmReult {
    DISARM_RESULT_SUCCESS               = 0, // Successfully disarmed
    DISARM_RESULT_ALREADY_DISARMED      = 1, // Alarm is already inactive
    DISARM_RESULT_PERMISSION_DENIED     = 2, // User does not have permission to disarm
    DISARM_RESULT_ALARM_NOT_FOUND       = 3, // Alarm is not connected to the server
    DISARM_RESULT_FAIL                  = 4, // Disarm failed inside alarm client
};

class AlarmApp : public App
{
public:
    void Start();

    bool IsArmed();
    void Toggle();

    uint32_t Disarm();
};
