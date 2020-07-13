#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "Arduino.h"

#if CONFIG_USE_WIFI
#include <WiFi.h>
#include <WiFiMulti.h>
#endif

#if CONFIG_USE_ETH
#include <ETH.h>
#endif

#if CONFIG_USE_ETH || CONFIG_USE_WIFI
void WiFiEvent(WiFiEvent_t event);
#endif

void network_setup();
bool network_loop();

#endif
