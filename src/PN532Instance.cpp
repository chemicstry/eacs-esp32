#include "PN532Instance.h"

// Use Serial2 of ESP32
HardwareSerial PN532Serial(CONFIG_READER_UART_NUM);

// Serial interface
PN532_HSU PN532HSU(PN532Serial);

// Extended library which allows card extensions
PN532Extended NFC(PN532HSU);
