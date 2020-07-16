# eacs-esp32
Extensible Access Control System. ESP32 based door controller.

## Description

Software is written using as much C++11 (including threading) features as possible so it should be easily portable to other platforms. Two libraries (PN532 and WebSockets) are arduino only and may require substitutions on unsupported platforms.

## Specifications
* Communications
  * WiFi
  * Ethernet (tested with Olimex ESP32-EVB board)
* NFC reader
  * PN532 (default is pin 16 RX, 17 TX on ESP32 side)

## Requirements
- [eacs-server](https://github.com/chemicstry/eacs-server) to accept connections from esp32 modules.

## Installation instructions

This project is based on platform.io so any IDE supporting it should work. There are no special compile instructions, just make sure to clone repository recursively (with git submodules).
