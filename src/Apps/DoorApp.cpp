#include "DoorApp.h"

#include "esp_log.h"
static const char* TAG = "DoorApp";

#include "../PN532Instance.h"
#include "../Utils.h"

static const uint8_t RELAY_PIN = CONFIG_RELAY_PIN;
static const uint8_t BUZZER_PIN = CONFIG_BUZZER_PIN;
static const uint8_t READER_BUTTON_PIN = CONFIG_READER_BUTTON_PIN;
static const Milliseconds READER_BUTTON_TIMEOUT = Milliseconds(1000);
static const uint8_t EXIT_BUTTON_PIN = CONFIG_EXIT_BUTTON_PIN;
static const Milliseconds EXIT_BUTTON_TIMEOUT = Milliseconds(1000);

static const Milliseconds ACTIVATION_DURATION = Milliseconds(CONFIG_ACTIVATION_DURATION);
static const Milliseconds BEEP_BEFORE_DEACTIVATION_DURATION = Milliseconds(CONFIG_BEEP_BEFORE_DEACTIVATION_DURATION);
static const Milliseconds BEEP_BEFORE_DEACTIVATION_INTERVAL = Milliseconds(CONFIG_BEEP_BEFORE_DEACTIVATION_INTERVAL);

static const Milliseconds TAG_READ_INTERVAL = Milliseconds(CONFIG_TAG_READ_INTERVAL);

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

void DoorApp::Start()
{
    App::Start();

    // Setup buttons
    pinMode(READER_BUTTON_PIN, INPUT);
    pinMode(EXIT_BUTTON_PIN, INPUT);

    // Setup buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    std::thread(std::bind(&DoorApp::RFIDThreadFn, this));
    std::thread(std::bind(&DoorApp::LogicThreadFn, this));
}

void DoorApp::RFIDThreadFn()
{
    // Configures PN532
    setupNFC();

    // Door relay
    pinMode(RELAY_PIN, OUTPUT);

    // Holds index of the current active tag
    // Used in matching transceive RPC call to tag
    static int currentTg = 0;

    // Transceive RPC method
    service.RPC->bind("rfid:transceive", [](std::string data) {
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

            if (!service.RPC->call("rfid:auth", BuildTagInfo(tgdata)))
            {
                ESP_LOGE(TAG, "Auth failed!");
                beep_long();

                #if CONFIG_INIT_KEY_ON_AUTH_FAIL
                    ESP_LOGI(TAG, "Trying to initialize key");
                    service.RPC->call("rfid:initKey", BuildTagInfo(tgdata));
                #endif

                continue;
            }
        } catch (const JSONRPC::RPCMethodException& e) {
            ESP_LOGE(TAG, "RPC call failed: %s", e.message.c_str());
            beep_long();
            continue;
        } catch (const JSONRPC::TimeoutException& e) {
            ESP_LOGE(TAG, "RPC call timed out");
            beep_long();
            continue;
        }

        ESP_LOGI(TAG, "Auth successful!");
        beep_short();

        lastActivationTime = Clock::now();

        // Extend time if last read time was within 5 sec window
        if (lastTagReadTime + TAG_READ_INTERVAL + Milliseconds(5000) > Clock::now())
            lastTagReadTime += TAG_READ_INTERVAL;
        else
            lastTagReadTime = Clock::now();

        // Wait until timeout
        while (lastTagReadTime + TAG_READ_INTERVAL > Clock::now())
            delay(100);
    }
}

void DoorApp::LogicThreadFn()
{
    static TimePoint exitButtonTimeout = TimePoint();

    ESP_LOGI(TAG, "Logic thread started");

    while(1)
    {
        /*if (!digitalRead(READER_BUTTON_PIN) && readerButtonTimeout+READER_BUTTON_TIMEOUT < millis())
        {
            readerButtonTimeout = millis();
            ESP_LOGI(TAG, "Reader button pressed");
            messageBus.RPC->notify("publish", "button");
        }*/

        if (!digitalRead(EXIT_BUTTON_PIN) && exitButtonTimeout + EXIT_BUTTON_TIMEOUT < Clock::now())
        {
            ESP_LOGI(TAG, "Exit button pressed");
            exitButtonTimeout = Clock::now();
            lastActivationTime = Clock::now();
            beep_short();

            json event;
            event["button"] = "exit";
            service.RPC->notify("rfid:logEvent", event);
        }

        TimePoint activationEndTime = lastActivationTime + ACTIVATION_DURATION;
        if (lastActivationTime != TimePoint() && activationEndTime > Clock::now()) {
            digitalWrite(RELAY_PIN, HIGH);

            if (activationEndTime - BEEP_BEFORE_DEACTIVATION_DURATION < Clock::now())
            {
                beep_short();
                delay(BEEP_BEFORE_DEACTIVATION_INTERVAL.count());
            }
        } else
            digitalWrite(RELAY_PIN, LOW);

        delay(10);
    }
}
