#include "OTAManager.h"
#include <Preferences.h>

// External dependencies that we need to reference
extern void sendLog(const String& log, int msgLevel = 3);
extern void setLEDColors(uint8_t red, uint8_t blue, uint8_t green, uint8_t white);
extern void display(const struct LEDstruct led);
extern LEDstruct colors;

// Log levels (should match main project)
#define NONE    0
#define ERROR   1
#define WARN    2
#define INFO    3
#define DEBUG   4
#define VERBOSE 5

// OTA-related global variables
int totalLength = 0;       // Total size of firmware
int currentLength = 0;     // Current size of written firmware
HTTPClient OTAclient;      // HTTP client for OTA downloads

void updateFirmware(uint8_t *data, size_t len) {
    // Function to update firmware incrementally
    // Buffer is declared to be 128 so chunks of 128 bytes
    // from firmware is written to device until server closes
    setLEDColors(127, 0, 0, 0); // Set the color to red -> updating firmware
    display(colors);
    Update.write(data, len);
    currentLength += len;
    // Print dots while waiting for update to finish
    Serial.print('.');
    // if current length of written firmware is not equal to total firmware size, repeat
    if(currentLength != totalLength) return;
    Update.end(true);
    sendLog("\nUpdate Success, Total Size: " + String(currentLength) + "\nRebooting...\n", INFO);
    
    // Restart ESP32 to see changes 
    ESP.restart();
}

bool fetchOTA(const String& url, bool persist) {
    bool status = false;
    String log;

    // Check if the URL starts with "http"
    if (!url.startsWith("http")) {
        sendLog("OTA URL must start with http:// or https:// received: " + url, ERROR);
        return false;
    }
    // Connect to external web server
    sendLog("Starting OTA update from URL: " + url, INFO);

    if (!persist) {
        // Clear previous wifi credentials
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
    }

    int resp = OTAclient.begin(url);
    if (resp != 1) {
        sendLog("OTAclient.begin() failed, return code: " + String(resp), ERROR);
        OTAclient.end();
        return false;
    }
    resp = OTAclient.GET();
    sendLog("OTA file response: " + String(resp), DEBUG);
    // If file is reachable, start downloading
    if(resp == 200) {
        // get length of document (is -1 when Server sends no Content-Length header)
        totalLength = OTAclient.getSize();
        // transfer to local variable
        int len = totalLength;
        // this is required to start firmware update process
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            sendLog("Update.begin() failed!", WARN);
            OTAclient.end();
            return false;
        }
        sendLog("FW Size: " + String(totalLength), DEBUG);
        // create buffer for read
        uint8_t buff[128] = { 0 };
        // get tcp stream
        WiFiClient * stream = OTAclient.getStreamPtr();
        // read all data from server
        sendLog("Updating firmware...", INFO);
        currentLength = 0; // Reset current length counter
        while(OTAclient.connected() && (len > 0 || len == -1)) {
            setLEDColors(255, 0, 0, 0); // Set the color to red -> updating firmware
            display(colors);

            // get available data size
            size_t size = stream->available();
            if(size) {
                // read up to 128 byte
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                // pass to function
                updateFirmware(buff, c);
                if(len > 0) {
                    len -= c;
                }
            }
            delay(1);
        }
        status = true;
    } else {
        sendLog("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location.", WARN);
        status = false;
    }
    OTAclient.end();
    return status;
}

void handleOTAEvent(const JsonDocument& jsonRxBuffer) {
    sendLog("Processing OTA event", INFO);
    //Serial.println("got MQTT OTA event");
    //serializeJson(jsonRxBuffer, Serial);
    if (jsonRxBuffer.containsKey("url")) {
        String otaUrl = jsonRxBuffer["url"].as<String>();
        Serial.println("Received OTA event, fetching firmware from: " + otaUrl);
        if (jsonRxBuffer.containsKey("persist")) {
            bool persist = jsonRxBuffer["persist"].as<bool>();
            //sendLog("Persist OTA: " + String(persist));
            fetchOTA(otaUrl, persist);
        } else {
            sendLog("No persist flag provided, defaulting to true.", INFO);
            fetchOTA(otaUrl);
        }
    } else {
        sendLog("OTA event received but no URL provided.", WARN);
    }
}
