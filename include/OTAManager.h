#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "SharedTypes.h"

// OTA-related global variables
extern int totalLength;       // Total size of firmware
extern int currentLength;     // Current size of written firmware
extern HTTPClient OTAclient;  // HTTP client for OTA downloads

// Function declarations
bool fetchOTA(const String& url, bool persist = true);
void updateFirmware(uint8_t *data, size_t len);
void handleOTAEvent(const JsonDocument& jsonRxBuffer);

#endif // OTA_MANAGER_H
