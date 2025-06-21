
#include <Arduino.h>
#include "EspMQTTClient.h"
#include "esp_system.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <secrets.h>
#include <time.h>
#include <FastLED.h>

//#define HOST "http://green.mattlovett.com/esp32fw.bin"

#define  redLEDs 0
#define  blueLEDs 1
#define  greenLEDs 2
#define  whiteLEDs 3
#define Logging "remote"

const int REDPIN = 16;
const int GREENPIN = 32;//17
const int BLUEPIN = 17;//21;
const int WHITEPIN = 21;//32;




//Time related variables
unsigned long startMillis;  
unsigned long currentMillis;
unsigned long eventTime;
unsigned long syncTime;
unsigned long deltaTime;
uint8_t debouceTime = 50;


//Device ID stuff
uint32_t macLow;
uint32_t macHigh; 
uint64_t fullMAC;

// NTP server to request time from
const char* ntpServer = "pool.ntp.org";
// Time offset in seconds (e.g., for UTC+1: 3600)
const long gmtOffset_sec = 0;
// Daylight offset in seconds (e.g., for daylight saving time: 3600)
const int daylightOffset_sec = 0;
unsigned long bootTimeMillis;

//static 

HTTPClient OTAclient;
EspMQTTClient* client;

char SSID[] =  WIFI_SSID;
char WIFIPASS[] = WIFI_PASSWORD;
char BROKER[] = MQTT_BROKER;
char MQTTu[] = MQTT_USER;   // Can be omitted if not needed
char MQTTp[] = MQTT_PASSWORD;
char mqttuser[] = "green1green1green1"; 
char deviceID[18];
char deviceChannel[40];    
char FW_Version[] = "1.0.0";
char HW_Version[]  = "1";


void display(struct LEDstruct);
void setLEDColors(uint8_t, uint8_t, uint8_t, uint8_t);
void showAnalogRGB(const CRGB&);
void sendJSON(const JsonDocument&, const char*);
bool fetchOTA(const String& HOST);
void syncNTP();
void colorBars();
void calcCurrentTimeMillis();
void printCurrentTimeMillis();
void removeColons(char*);
String getMacAddress();