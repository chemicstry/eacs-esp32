#include "WebSocketsClient.h"
#include "PN532Instance.h"
#include "JSONRPC/JSONRPC.h"
#include "Service.h"
#include "Utils.h"
#include "esp_log.h"
#include <thread>
#include "Arduino.h"
#include "network.h"
#include "driver/uart.h"
#include <chrono>

#if CONFIG_USE_MDNS
#include <ESPmDNS.h>
#endif

static const char* TAG = "main";

using namespace std::placeholders;
using json = nlohmann::json;

Service server("eacs-server");

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Milliseconds = std::chrono::milliseconds;
TimePoint lastTagReadTime = TimePoint();
TimePoint lastActivationTime = TimePoint();

#define RELAY_PIN CONFIG_RELAY_PIN
#define BUZZER_PIN CONFIG_BUZZER_PIN

void tone(int pin, int freq, int duration)
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

#define BUZZ_SHORT 150
#define BUZZ_LONG 800

void buzz(int duration)
{
    #if CONFIG_BUZZER_PWM
        tone(CONFIG_BUZZER_PIN, 2000, duration);
    #else
        digitalWrite(CONFIG_BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(CONFIG_BUZZER_PIN, LOW);
    #endif
}

void setupNFC()
{
    // Start PN532
    PN532Serial.begin(PN532_HSU_SPEED, SERIAL_8N1, CONFIG_READER_RX_PIN, CONFIG_READER_TX_PIN);
    NFC.begin();

    // Get PN532 firmware version
    GetFirmwareVersionResponse version;
    if (!NFC.GetFirmwareVersion(version)) {
        ESP_LOGE(TAG, "Unable to fetch PN532 firmware version");
        ESP.restart();
    }

    // configure board to read RFID tags and prevent going to sleep
    if (!NFC.SAMConfig()) {
        ESP_LOGE(TAG, "NFC SAMConfig failed");
        ESP.restart();
    }
    
    // Print chip data
    ESP_LOGI(TAG, "Found chip PN5%02X", version.IC);
    ESP_LOGI(TAG, "Firmware version: %#04X", version.Ver);
    ESP_LOGI(TAG, "Firmware revision: %#04X", version.Rev);
    ESP_LOGI(TAG, "Features: ISO18092: %d, ISO14443A: %d, ISO14443B: %d",
        (bool)version.Support.ISO18092,
        (bool)version.Support.ISO14443_TYPEA,
        (bool)version.Support.ISO14443_TYPEB);
    
    // Set the max number of retry attempts to read from a card
    // This prevents us from waiting forever for a card, which is
    // the default behaviour of the PN532.
    if (!NFC.SetPassiveActivationRetries(0x00)) {
        ESP_LOGE(TAG, "NFC SetPassiveActivationRetries failed");
        ESP.restart();
    }
}

// Builds tagInfo JSON object
json BuildTagInfo(const PN532Packets::TargetDataTypeA& tgdata)
{
    json taginfo;
    taginfo["ATQA"] = tgdata.ATQA;
    taginfo["SAK"] = tgdata.SAK;
    // Hex encoded strings
    taginfo["ATS"] = BinaryDataToHexString(tgdata.ATS);
    taginfo["UID"] = BinaryDataToHexString(tgdata.UID);
    return taginfo;
}

// Waits for RFID tag and then authenticates
std::thread RFIDThread;
void RFIDThreadFunc2();
void RFIDThreadFunc()
{
    RFIDThreadFunc2();
}
void RFIDThreadFunc2()
{
    // Configures PN532
    setupNFC();

    // Door relay
    pinMode(RELAY_PIN, OUTPUT);

    // Holds index of the current active tag
    // Used in matching transceive RPC call to tag
    static int currentTg = 0;

    // Transceive RPC method
    server.RPC->bind("rfid:transceive", [](std::string data) {
        // Parse hex string to binary array
        BinaryData buf = HexStringToBinaryData(data);

        // Create interface with tag
        TagInterface tif = NFC.CreateTagInterface(currentTg);

        // Send
        if (tif.Write(buf))
            throw JSONRPC::RPCMethodException(1, "Write failed");

        // Receive
        if (tif.Read(buf) < 0)
            throw JSONRPC::RPCMethodException(2, "Read failed");

        // Convert back to hex encoded string
        return BinaryDataToHexString(buf);
    });

    while(1)
    {
        // Release any previously connected target
        NFC.InRelease(currentTg);

        // Finds nearby ISO14443 Type A tags
        InListPassiveTargetResponse resp;
        if (!NFC.InListPassiveTarget(resp, 1, BRTY_106KBPS_TYPE_A))
        {
            ESP_LOGE(TAG, "NFC InListPassiveTarget failed");
            ESP.restart();
        }

        // Only read one tag at a time
        if (resp.NbTg != 1)
        {
            // Yield to kernel
            delay(10);
            continue;
        }
        
        ESP_LOGI(TAG, "Detected RFID tag");

        // For parsing response data
        ByteBuffer buf(resp.TgData);

        // Parse as ISO14443 Type A target
        PN532Packets::TargetDataTypeA tgdata;
        buf >> tgdata;

        // Set current active tag
        currentTg = tgdata.Tg;

        // Convert UID to hex encoded string
        std::string UID = BinaryDataToHexString(tgdata.UID);
        ESP_LOGI(TAG, "Tag UID: %s", UID.c_str());
        
        // Initiate authentication
        try {
            ESP_LOGI(TAG, "Performing RPC authentication...");

            if (!server.RPC->call("rfid:auth", BuildTagInfo(tgdata)))
            {
                ESP_LOGE(TAG, "Auth failed!");
                buzz(BUZZ_LONG);

                #if CONFIG_INIT_KEY_ON_AUTH_FAIL
                    ESP_LOGI(TAG, "Trying to initialize key");
                    server.RPC->call("rfid:initKey", BuildTagInfo(tgdata));
                #endif

                continue;
            }
        } catch (const JSONRPC::RPCMethodException& e) {
            ESP_LOGE(TAG, "RPC call failed: %s", e.message.c_str());
            buzz(BUZZ_LONG);
            continue;
        } catch (const JSONRPC::TimeoutException& e) {
            ESP_LOGE(TAG, "RPC call timed out");
            buzz(BUZZ_LONG);
            continue;
        }

        ESP_LOGI(TAG, "Auth successful!");
        buzz(BUZZ_SHORT);

        lastActivationTime = Clock::now();

        // Extend time if last read time was within 5 sec window
        if (lastTagReadTime+Milliseconds(CONFIG_TAG_READ_INTERVAL+5000) > Clock::now())
            lastTagReadTime += Milliseconds(CONFIG_TAG_READ_INTERVAL);
        else
            lastTagReadTime = Clock::now();

        // Wait until timeout
        while (lastTagReadTime+Milliseconds(CONFIG_TAG_READ_INTERVAL) > Clock::now())
            delay(100);
    }
}

#define READER_BUTTON_PIN CONFIG_READER_BUTTON_PIN
#define READER_BUTTON_TIMEOUT 1000
#define EXIT_BUTTON_PIN CONFIG_EXIT_BUTTON_PIN
#define EXIT_BUTTON_TIMEOUT 1000

std::thread LogicThread;
void LogicThreadFun()
{
    ESP_LOGI(TAG, "Logic thread started");

    while (1)
    {
        // Ignore zero timepoint (after bootup)
        if (lastActivationTime == TimePoint())
            continue;

        TimePoint activationEndTime = lastActivationTime+Milliseconds(CONFIG_ACTIVATION_DURATION);

        if (activationEndTime > Clock::now()) {
            digitalWrite(RELAY_PIN, HIGH);

            if (activationEndTime-Milliseconds(CONFIG_BEEP_BEFORE_DEACTIVATION_DURATION) < Clock::now())
            {
                buzz(BUZZ_SHORT);
                delay(CONFIG_BEEP_BEFORE_DEACTIVATION_INTERVAL);
            }
        } else
            digitalWrite(RELAY_PIN, LOW);

        delay(100);
    }
}

std::thread GPIOThread;
void GPIOThreadFunc()
{
    //static uint64_t readerButtonTimeout = 0;
    static TimePoint exitButtonTimeout = TimePoint();

    ESP_LOGI(TAG, "GPIO thread started");

    while(1)
    {
        /*if (!digitalRead(READER_BUTTON_PIN) && readerButtonTimeout+READER_BUTTON_TIMEOUT < millis())
        {
            readerButtonTimeout = millis();
            ESP_LOGI(TAG, "Reader button pressed");
            messageBus.RPC->notify("publish", "button");
        }*/

        if (!digitalRead(EXIT_BUTTON_PIN) && exitButtonTimeout+Milliseconds(EXIT_BUTTON_TIMEOUT) < Clock::now())
        {
            ESP_LOGI(TAG, "Exit button pressed");
            exitButtonTimeout = Clock::now();
            lastActivationTime = Clock::now();
            buzz(BUZZ_SHORT);

            json event;
            event["button"] = "exit";
            server.RPC->notify("rfid:logEvent", event);
        }

        delay(10);
    }
}

std::thread NetworkThread;
void NetworkThreadFunc() {
    while(1)
    {
        if (network_loop())
        {
            try {
                server.transport.update();
            } catch(const JSONRPC::MethodNotFoundException& e) {
                ESP_LOGE(TAG, "RPC method '%s' not found", e.method.c_str());
            } catch(const JSONRPC::InvalidJSONRPCException& e) {
                ESP_LOGE(TAG, "RPC received invalid json");
            }
        }

        // Yield to kernel to prevent triggering watchdog
        delay(10);
    }
}


extern "C" void app_main() {
    initArduino();

    // Start mDNS
    #if CONFIG_USE_MDNS
        if (!MDNS.begin("eacs_esp32")) {
            ESP_LOGE(TAG, "mDNS responder setup failed.");
            ESP.restart();
        }
    #endif

    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // Configures WiFi and/or Ethernet
    network_setup();

    // Configures task state reporting if enabled in menuconfig
    EnableTaskStats();

    // Setup buttons
    pinMode(READER_BUTTON_PIN, INPUT);
    pinMode(EXIT_BUTTON_PIN, INPUT);

    // Setup buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    #if CONFIG_SERVER_USE_MDNS
        server.MDNSQuery();
    #else
        server.host = CONFIG_SERVER_HOST;
        server.port = CONFIG_SERVER_PORT;
    #endif

    server.SetAuthToken(CONFIG_SERVER_TOKEN);

    #if CONFIG_SERVER_SSL
        server.fingerprint = CONFIG_SERVER_SSL_FINGERPRINT;
        server.BeginTransportSSL();
    #else
        server.BeginTransport();
    #endif 

    // Starts network thread
    NetworkThread = std::thread(NetworkThreadFunc);

    // Starts main RFID thread
    RFIDThread = std::thread(RFIDThreadFunc);

    // Starts GPIO thread (button checking)
    GPIOThread = std::thread(GPIOThreadFunc);

    // Starts Logic Thread (activation timeouts)
    LogicThread = std::thread(LogicThreadFun);
}
