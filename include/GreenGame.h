
#include <Arduino.h>
#include "EspMQTTClient.h"
#include "esp_system.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <secrets.h>
#include <time.h>
//#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>



// Define log levels
#define NONE    0
#define ERROR   1
#define WARN    2
#define INFO    3
#define DEBUG   4
#define VERBOSE 5

#define logLevelSerial  DEBUG // Set the default log level
#define logLevelMQTT  INFO // Set the default MQTT log level

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

String ssid;
String pass;
String deviceName; 

// NTP server to request time from
const char* ntpServer = "pool.ntp.org";
// Time offset in seconds (e.g., for UTC+1: 3600)
const long gmtOffset_sec = 0;
// Daylight offset in seconds (e.g., for daylight saving time: 3600)
const int daylightOffset_sec = 0;
unsigned long bootTimeMillis;

//static 

//===================================== Structure Def ============================================

struct Button {
  volatile unsigned long touchTime = 0;
  volatile unsigned long delta = 9999999;
  volatile bool pressed = false;
};

struct LEDstruct {
  uint8_t redBrightness = 0;
  uint8_t greenBrightness = 0;
  uint8_t blueBrightness = 0;
  uint8_t whiteBrightness = 0;
};

struct Event {
  bool newEvent = false;
  unsigned long eventTime = 0;
  uint64_t deviceID = 0;
};

//=================================== End Structure Def ==========================================

HTTPClient OTAclient;
EspMQTTClient* client;
WebServer server(80);
DNSServer dnsServer; // DNS server for captive portal
Preferences prefs;

Button touchBtn;
LEDstruct colors;
Event event;

// Set custom IP for the SoftAP before starting it
IPAddress apIP(192, 168, 4, 1);      // Default ESP32 AP IP, change as needed
IPAddress gateway(192, 168, 4, 1);   // Gateway (usually same as IP)
IPAddress subnet(255, 255, 255, 0);  // Subnet mask


char SSID[] =  WIFI_SSID;
char WIFIPASS[] = WIFI_PASSWORD;
char BROKER[] = MQTT_BROKER;
char MQTTu[] = MQTT_USER;   // Can be omitted if not needed
char MQTTp[] = MQTT_PASSWORD;
char mqttuser[] = "green1green1green1"; 
char deviceID[18];
char deviceChannel[40];    
char FW_Version[] = "1.0.6";
char HW_Version[]  = "1";

void IRAM_ATTR touchEvent(void);
void display(struct LEDstruct);
void setLEDColors(uint8_t, uint8_t, uint8_t, uint8_t);
void sendJSON(const JsonDocument&, const char*);
bool fetchOTA(const String& HOST, bool persist = true);
void syncNTP();
void colorBars();
void calcCurrentTimeMillis();
void printCurrentTimeMillis();
void removeColons(char*);
void startProvisioningAP();
void handleSave();
void handleRoot();
float cubicEaseInOut(float t);
int interpolate(int start, int end, float t);
void hsvToRgb(float h, float s, float v, float& r, float& g, float& b);
void handleNotFound();
void factoryReset();
void sendLog(const String& log, int msgLevel = INFO);
String getMacAddress();

// HTML page served for Wi-Fi provisioning

const char* psavePage = R"rawliteral(
        "text/html",
        "<html><body>
        <h3>Credentials saved!</h3>"
        "<p>Rebooting...</p>
            <img class="center" src="data:image/png;base64,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" />
        </body></html>"
    )rawliteral";

const char* provisioningPage = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
    <title>Fungers WiFi Setup</title>
    <style>
        :root {
            --color-background:    #FFFDF9;
            --color-text:          #232323;
            --color-primary:       #FF5722;
            --color-accent:        #4CAF50;
            --color-accent-light:  #66BB6A;
            --color-finger:        #FBC68F;
            --font-sans:  'Poppins', sans-serif;
            --font-display: 'Nunito', sans-serif;
            --radius-pill: 9999px;
            --spacing-sm:  .5rem;
            --spacing:     1rem;
            --spacing-lg:  1.5rem;
            --shadow-sm:   0 1px 3px rgba(0,0,0,0.1);
            --shadow-md:   0 4px 6px rgba(0,0,0,0.1);
        }
        *,
        *::before,
        *::after {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            background-color: var(--color-background);
            color: var(--color-text);
            padding: var(--spacing);
            font-family: var(--font-sans);
            font-size: 1rem;
            font-weight: 600;
            line-height: 1.6;
        }
        .logo-text {
            font-family: var(--font-sans);
            font-size: 2rem;
            color: var(--color-primary);
            text-align: center;
            margin-bottom: var(--spacing-lg);
            font-weight: 800;
        }
        .center { display: block; margin-left: auto; margin-right: auto; width: 50%; }
        .button {
            display: inline-block;
            padding: var(--spacing-sm) var(--spacing-lg);
            background-color: var(--color-accent);
            color: white;
            font-size: 1rem;
            font-weight: 600;
            font-family: var(--font-sans);
            border: none;
            border-radius: var(--radius-pill);
            box-shadow: var(--shadow-sm);
            cursor: pointer;
            transition: background-color 0.2s, transform 0.1s, box-shadow 0.2s;
        }
        .button:hover {
            background-color: var(--color-accent-light);
            box-shadow: var(--shadow-md);
            transform: translateY(-1px);
        }
        .button:active {
            transform: translateY(0);
            box-shadow: var(--shadow-sm);
        }
        .card {
            background: white;
            border-radius: 8px;
            box-shadow: var(--shadow-md);
            padding: var(--spacing);
            max-width: 400px;
            margin: 0 auto;
        }
        .text-center { text-align: center; }
        .mt-1 { margin-top: var(--spacing); }
        .mb-1 { margin-bottom: var(--spacing); }
        .p-1  { padding: var(--spacing); }

        /* Center input fields in the card */
        .form-group {
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .form-group label {
            width: 100%;
            text-align: center;
            margin-bottom: 0.25rem;
        }
        .form-group input {
            width: 80%;
            max-width: 250px;
            margin: 0 auto;
            display: block;
            text-align: center;
        }
    </style>
</head>
<body>
    <img class="center" src="data:image/png;base64,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" />
    <h2 class="logo-text">Configure WiFi</h2>

    <div class="card">
        <form action="/save" method="POST">
            
            <div class="mb-1 form-group">
                <label for="ssid">SSID</label>
                <input id="ssid" name="ssid" type="text" class="p-1" />
            </div>

            <div class="mb-1 form-group">
                <label for="pass">Password</label>
                <input id="pass" name="pass" type="password" class="p-1" />
            </div>

            <div class="mb-1 form-group">
                <label for="deviceName">User Name</label>
                <input id="deviceName" name="deviceName" type="text" class="p-1" />
            </div>

            <div class="text-center mt-1">
                <button type="submit" class="button">Save &amp; Reboot</button>
            </div>

        </form>
    </div>

</body>
</html>

)rawliteral";
