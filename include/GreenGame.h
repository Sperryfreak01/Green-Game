




#include <Arduino.h>
#include "EspMQTTClient.h"
#include "esp_system.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <secrets.h>

//#define HOST "http://green.mattlovett.com/esp32fw.bin"


#define  redLEDs 0
#define  blueLEDs 1
#define  greenLEDs 2
#define  whiteLEDs 3
#define Logging "remote"

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


//static 

HTTPClient OTAclient;
EspMQTTClient* client;

char SSID[] =  WIFI_SSID;
char WIFIPASS[] = WIFI_PASSWORD;
char BROKER[] = MQTT_BROKER;
char MQTTu[] = MQTT_USER;   // Can be omitted if not needed
char MQTTp[] = MQTT_PASSWORD;
char mqttuser[] = "green01234567"; 
char deviceID[17];
char deviceChannel[40];    
char FW_Version[] = "0.1";
char HW_Version[]  = "1";


void display(struct LEDstruct);
void sendJSON(const JsonDocument&, const char*);
bool fetchOTA(const String& HOST);
String getMacAddress();