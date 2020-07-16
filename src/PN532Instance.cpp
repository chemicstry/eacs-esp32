#include "PN532Instance.h"

#include "esp_log.h"
static const char* TAG = "NFC";

// Use Serial2 of ESP32
HardwareSerial PN532Serial(CONFIG_READER_UART_NUM);

// Serial interface
PN532_HSU PN532HSU(PN532Serial);

// Extended library which allows card extensions
PN532Extended NFC(PN532HSU);

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
