#pragma once

#include "../Service.h"

class App
{
public:
    App();

    void Start();

protected:
    Service service;

private:
    void SetupNetwork();
    void UpdateNetwork();
    void NetworkThreadFn();
};
