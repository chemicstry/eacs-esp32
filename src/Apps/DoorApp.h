#pragma once

#include "App.h"
#include "../TimeUtils.h"

class DoorApp : public App
{
public:
    void Start();

private:
    void RFIDThreadFn();
    void LogicThreadFn();

    TimePoint lastTagReadTime;
    TimePoint lastActivationTime;
};
