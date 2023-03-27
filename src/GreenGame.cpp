/*
  SimpleMQTTClient.ino
  The purpose of this exemple is to illustrate a simple handling of MQTT and Wifi connection.
  Once it connects successfully to a Wifi network and a MQTT broker, it subscribe to a topic and send a message to it.
  It will also send a message delayed 5 seconds later.
*/
#define ARDUINOJSON_USE_LONG_LONG 1

#include <GreenGame.h>
#include <secrets.h>
#include <HTTPClient.h>
#include <Update.h>
#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "esp_system.h"


void IRAM_ATTR touchEvent(void);
void display(struct LEDstruct);
void sendJSON(const JsonDocument&);
//===================================== Object Def ============================================

HTTPClient OTAclient;

EspMQTTClient client(
  WIFI_SSID, // TODO #1 Change to allow user to set wifi password
  WIFI_PASSWORD,
  "mattlovett.com",  // MQTT Broker server ip
  MQTT_USER,   // Can be omitted if not needed
  MQTT_PASSWORD,   // Can be omitted if not needed
  "greengame3",     // Client name that uniquely identify your device  #TODO #2 make MQTT login client name dynamic somehow
  1883              // The MQTT port, default to 1883. this line can be omitted
);
//=================================== End Object Def ==========================================

//===================================== Structure Def ============================================

struct Button {
	volatile unsigned long touchTime = 0;
  volatile unsigned long delta = 9999999;
	volatile bool pressed = false;
};

struct LEDstruct {
 	uint8_t redBrightness = 0;
  uint8_t blueBrightness = 0;
	uint8_t greenBrightness = 0;
	uint8_t whiteBrightness = 0;
};


struct Event {
  bool newEvent = false;
  unsigned long eventTime = 0;
  uint64_t deviceID = 0;
};

//=================================== End Structure Def ==========================================

//===================================== Variable  Def ============================================
// Global variables
int totalLength;       //total size of firmware
int currentLength = 0; //current size of written firmware

//Time related variables
unsigned long startMillis;  
unsigned long currentMillis;
unsigned long eventTime;
unsigned long syncTime;
unsigned long deltaTime;

Button touchBtn;
LEDstruct colors;
Event event;

//Device ID stuff
uint32_t macLow;
uint32_t macHigh; 
uint64_t fullMAC;
String deviceID = "unknown";
String deviceChan = String("greengame/events/" + deviceID);

//=================================== End Variable Def ==========================================

void setup()
{ //setup all the LED control pin
  pinMode(redLEDs, OUTPUT);
  digitalWrite(redLEDs, LOW);

  pinMode(blueLEDs, OUTPUT);
  digitalWrite(blueLEDs, LOW);

  pinMode(greenLEDs, OUTPUT);
  digitalWrite(greenLEDs, LOW);

  pinMode(whiteLEDs, OUTPUT);
  digitalWrite(whiteLEDs, LOW);

  //Configure the interupt for the cap touch sensor
  pinMode(D4, INPUT);
  attachInterrupt(digitalPinToInterrupt(D4), touchEvent, RISING);

  //Retrieve and build the MAC strign so we can use it later as a MQTT device identifier
  macLow  = ESP.getEfuseMac() & 0xFFFFFFFF; 
  macHigh = ( ESP.getEfuseMac() >> 32 ) % 0xFFFFFFFF;
  fullMAC = ESP.getEfuseMac();
  deviceID = String(macLow) + String(macHigh);
  String deviceChan = String("greengame/events/" + deviceID); //MQTT channel for this specific device, used to post status msgs, logs, targeted OTAs, etc.
  Serial.printf("Full: %llu \n",fullMAC);

  Serial.begin(115200);

  // Optional functionalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridden with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage("TestClient/lastwill", "Disconnected");  // You can activate the retain flag by setting the third parameter to true
  
  //Zero out the system time, we will use this time to compute who the winner is on a MQTT touch event
  syncTime=millis();
  startMillis = millis();  //initial start time
}


void loop()
{
  client.loop(); //Wifi keep alive

  if (touchBtn.pressed) {
        
    deltaTime = touchBtn.touchTime - syncTime;
		Serial.printf("touch Event at delta of %lu ms \n", touchBtn.delta);
    client.publish("greengame/events/", "touch Event at delta of %lu ms \n", touchBtn.delta);

    colors.redBrightness = 0;
    colors.blueBrightness = 0;
    colors.greenBrightness = 255;
    colors.whiteBrightness = 0;

    display(colors);
    
    // Publish a message 
    StaticJsonDocument<200> jsonTxBuffer;
    jsonTxBuffer["event"] = "touch";
    jsonTxBuffer["device"] = fullMAC; 
    jsonTxBuffer["delta"] = touchBtn.delta;
    sendJSON(jsonTxBuffer); 

    touchBtn.pressed = false;
	}

  if (event.newEvent) {
    //deltaTime = event.eventTime - syncTime;

    Serial.println(touchBtn.touchTime - syncTime);
    if (event.eventTime < (touchBtn.touchTime - syncTime)){
      Serial.println("they win");
      Serial.print("the event occured at: ");
      Serial.println(event.eventTime);
      Serial.printf("last touch Event at %lu \n", touchBtn.delta);
      //the event happened sooner than our last touch event, filters out delayed messages?? thats what I am telling myself.
      //set the color to red and sync the time
      colors.redBrightness = 0;
      colors.blueBrightness = 0;
      colors.greenBrightness = 0;
      colors.whiteBrightness = 255;

      StaticJsonDocument<200> jsonTxBuffer;
      jsonTxBuffer["event"] = "sync";
      jsonTxBuffer["device"] = fullMAC; 
      sendJSON(jsonTxBuffer);
    }
    else if (event.eventTime >= (touchBtn.touchTime - syncTime)){
      Serial.println("they lose");
      Serial.print("the event occured at: ");
      Serial.println(event.eventTime);
      Serial.print("the last touch occured at: ");
      //the event happened later than our last touch event, filters out delayed messages?? thats what I am telling myself.
      //set the color to red and sync the time
      colors.redBrightness = 128; //just so I can debug a losers message coming in

      StaticJsonDocument<200> jsonTxBuffer;
      jsonTxBuffer["event"] = "sync";
      jsonTxBuffer["device"] = fullMAC; 
      sendJSON(jsonTxBuffer);
    }

    display(colors);
    event.newEvent = false;    
  }
}



void IRAM_ATTR touchEvent(){
  touchBtn.touchTime = millis();
  touchBtn.delta = touchBtn.touchTime - syncTime;
  //TODO add some kind of touch backoff to stop people from slapping it over and over again, or fluttering events 
  touchBtn.pressed = true;
}

void synchronize(){
  syncTime = millis();
}

void recieveEvents(const String& msg){
  /*even types:
    0: No Event/Unknown
    FF: other event
    2: OTA
    3: Touch
    4: Sync
    5: Disable
  */
  StaticJsonDocument<200> jsonRxBuffer;
  DeserializationError error = deserializeJson(jsonRxBuffer, msg);
  if (error){
      Serial.println("event did not contain JSON");
      Serial.print(msg);
  }
  else if(jsonRxBuffer.containsKey("event")){
    if(jsonRxBuffer["event"] == "OTA"){
    }  
    else if(jsonRxBuffer["event"] == "touch"){
      Serial.println("got MQTT touch event");
      if (jsonRxBuffer["time"] != fullMAC){
        event.newEvent = true;
        event.eventTime = jsonRxBuffer["delta"];
        event.deviceID = jsonRxBuffer["device"];
      }
      else{
        Serial.println("this was our event");
      }
      return;      
    }
    else if(jsonRxBuffer["event"] == "sync"){ //someone just  processed a wining event - everyone clear thier timers to sync up
      Serial.println("syncing time");   //will fire off everytime a player processes, this will not scale and will pump traffic
      synchronize();
      //if(jsonRxBuffer["device"] != deviceID){} //no need to sync on our own event only others...wait maybe we do so everyone has round trip latency...test it...
      //  synchronize()
      //}
    }
  }
  else {
    Serial.println("unknown JSON event type");
    //Serial.print(jsonRxBuffer);
  }

 }


void display(struct LEDstruct led) {
  analogWrite(redLEDs, led.redBrightness);
  analogWrite(blueLEDs, led.blueBrightness);
  analogWrite(greenLEDs, led.greenBrightness);
  analogWrite(whiteLEDs, led.whiteBrightness);
}



void updateFirmware(uint8_t *data, size_t len){
  // Function to update firmware incrementally
  // Buffer is declared to be 128 so chunks of 128 bytes
  // from firmware is written to device until server closes

  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if(currentLength != totalLength) return;
  Update.end(true);
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes 
  ESP.restart();
}


void sendJSON(const JsonDocument& json){
    char msg[128];
    int b =serializeJson(json, msg);
    Serial.println("message length = ");
    Serial.print(b,DEC);
    client.publish("greengame/events/", msg); // You can activate the retain flag by setting the third parameter to true  
}







void onConnectionEstablished(){
  // This function is called once everything is connected (Wifi and MQTT), is used to register callbacks for MQTT messages recieved
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("greengame/events/", recieveEvents);

  // Publish a message to "mytopic/test"
  
  client.publish(deviceChan, "Connected"); // You can activate the retain flag by setting the third parameter to true

}


bool fetchOTA(char* HOST) {
  bool status;
  // Connect to external web server
  OTAclient.begin(HOST);
  // Get file, just to check if each reachable
  int resp = OTAclient.GET();
  Serial.print("Response: ");
  Serial.println(resp);
  // If file is reachable, start downloading
  if(resp == 200){
      // get length of document (is -1 when Server sends no Content-Length header)
      totalLength = OTAclient.getSize();
      // transfer to local variable
      int len = totalLength;
      // this is required to start firmware update process
      Update.begin(UPDATE_SIZE_UNKNOWN);
      Serial.printf("FW Size: %u\n",totalLength);
      // create buffer for read
      uint8_t buff[128] = { 0 };
      // get tcp stream
      WiFiClient * stream = OTAclient.getStreamPtr();
      // read all data from server
      Serial.println("Updating firmware...");
      while(OTAclient.connected() && (len > 0 || len == -1)) {
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
      status = 1;
      }
  }else{
    Serial.println("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location #defined in HOST.");
    status = 0;
  }
  OTAclient.end();
  return status;
}
