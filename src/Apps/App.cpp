#include "App.h"
#include "Arduino.h"

#include "esp_log.h"
static const char* TAG = "App";

#if CONFIG_USE_MDNS
#include <ESPmDNS.h>
#endif

#if !CONFIG_USE_ETH && !CONFIG_USE_WIFI
    #warning "No netwokr connection enabled. Both WiFi and Ethernet disabled."
#endif

#if CONFIG_USE_WIFI
#include <WiFi.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#endif

#if CONFIG_USE_ETH
#include <ETH.h>
#endif

#if CONFIG_USE_ETH || CONFIG_USE_WIFI
void WiFiEvent(WiFiEvent_t event)
{
    switch (event) {
        #if CONFIG_USE_ETH
        case SYSTEM_EVENT_ETH_START:
            Serial.println("ETH Started");
            //set eth hostname here
            ETH.setHostname("esp32-ethernet");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH MAC: ");
            Serial.print(ETH.macAddress());
            Serial.print(", IPv4: ");
            Serial.print(ETH.localIP());
            if (ETH.fullDuplex()) {
                Serial.print(", FULL_DUPLEX");
            }
            Serial.print(", ");
            Serial.print(ETH.linkSpeed());
            Serial.println("Mbps");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            break;
        #endif
        default:
            break;
    }
}
#endif

App::App(): service("eacs-server")
{
}

void App::Start()
{
    SetupNetwork();

    //std::thread(std::bind(&App::NetworkThreadFn, this));
}

void App::SetupNetwork()
{
    #if CONFIG_USE_ETH || CONFIG_USE_WIFI
        // Enable wifi event callbacks
        WiFi.onEvent(WiFiEvent);
    #endif

    #if CONFIG_USE_WIFI
        wifiMulti.addAP(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

        ESP_LOGI(TAG, "Connecting Wifi...");
        if (wifiMulti.run() == WL_CONNECTED)
            ESP_LOGI(TAG, "WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
    #endif

    #if CONFIG_USE_ETH
        ETH.begin();
    #endif

    // Start mDNS
    #if CONFIG_USE_MDNS
        if (!MDNS.begin("eacs_esp32")) {
            ESP_LOGE(TAG, "mDNS responder setup failed.");
            ESP.restart();
        }
    #endif

    #if CONFIG_SERVER_USE_MDNS
        service.MDNSQuery();
    #else
        service.host = CONFIG_SERVER_HOST;
        service.port = CONFIG_SERVER_PORT;
    #endif

    #if CONFIG_SERVER_SSL
        service.fingerprint = CONFIG_SERVER_SSL_FINGERPRINT;
        service.BeginTransportSSL();
    #else
        service.BeginTransport();
    #endif 
}

void App::UpdateNetwork()
{
    #if CONFIG_USE_WIFI
        if (wifiMulti.run() != WL_CONNECTED) {
            static uint32_t lastLog = 0;
            if (lastLog + 1000 < millis())
            {
                ESP_LOGE(TAG, "WiFi not connected!");
                lastLog = millis();
            }

            return;
        }
    #endif

    try {
        service.transport.update();
    } catch(const JSONRPC::MethodNotFoundException& e) {
        ESP_LOGE(TAG, "RPC method '%s' not found", e.method.c_str());
    } catch(const JSONRPC::InvalidJSONRPCException& e) {
        ESP_LOGE(TAG, "RPC received invalid json");
    }
}

void App::NetworkThreadFn()
{
    while(1) {
        UpdateNetwork();

        // Yield to kernel to prevent triggering watchdog
        delay(10);
    }
}