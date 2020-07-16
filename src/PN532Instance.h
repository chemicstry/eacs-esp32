#pragma once

#include <PN532_HSU.h>
#include <PN532Extended.h>

// Use Serial2 of ESP32
extern HardwareSerial PN532Serial;

// Serial interface
extern PN532_HSU PN532HSU;

// Extended library which allows card extensions
extern PN532Extended NFC;

void setupNFC();
