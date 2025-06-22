#define ARDUINOJSON_USE_LONG_LONG 1
//#define DEBUG_ESP_DNS_SERVER 1

#include <GreenGame.h>
#include <DNSServer.h>

void IRAM_ATTR touchEvent(void);

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

//===================================== Variable  Def ============================================
// Global variables
int totalLength;       //total size of firmware
int currentLength = 0; //current size of written firmware

// Webserver on port 80
WebServer server(80);
// Preferences namespace
Preferences prefs;

Button touchBtn;
LEDstruct colors;
Event event;

// Set custom IP for the SoftAP before starting it
IPAddress apIP(192, 168, 4, 1);      // Default ESP32 AP IP, change as needed
IPAddress gateway(192, 168, 4, 1);   // Gateway (usually same as IP)
IPAddress subnet(255, 255, 255, 0);  // Subnet mask
DNSServer dnsServer;

// Duration of one full transition in milliseconds
const unsigned long TRANSITION_DURATION = 3000;

unsigned long startTime = 0;
bool forward = true;
//=================================== End Variable Def ==========================================

void setup()
//setup all the LED control pin
{
  startTime = millis();

  // Initialize the LED pins
  pinMode(REDPIN,   OUTPUT);
  digitalWrite(REDPIN, LOW);
  pinMode(BLUEPIN,  OUTPUT);
  digitalWrite(BLUEPIN, LOW);
  pinMode(GREENPIN, OUTPUT);
  digitalWrite(GREENPIN, LOW);
  pinMode(WHITEPIN, OUTPUT);
  digitalWrite(WHITEPIN, LOW);

  //Configure the interupt for the cap touch sensor
  pinMode(4, INPUT);
  attachInterrupt(digitalPinToInterrupt(4), touchEvent, RISING);

  Serial.begin(115200);

  // Try to read stored credentials
  prefs.begin("wifi", true);
  //Clear preferences for testing, remove in production 
  //#TODO move this to an MQTT parsed message
  //prefs.begin("wifi");
  prefs.clear(); // Clear preferences for testing, remove in production #TODO move this to an MQTT parsed message

  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String deviceName = prefs.getString("deviceName", "");
  prefs.end();

  if (ssid.length() > 0 && pass.length() > 0) {
    Serial.printf("Found saved SSID '%s', attempting to connect...\n", ssid.c_str());
    //Retrieve and build the MAC strign so we can use it later as a MQTT device identifier
    String tmpMAC = getMacAddress();
    strcpy(deviceID, tmpMAC.c_str());
    removeColons(deviceID);

    //TODO eventually pull in the user from the captive portal configuration
    //strcpy (mqttuser,"");
    strcpy (mqttuser,deviceName.c_str());
    //strcat (mqttuser, "green");
    //strcat (mqttuser, deviceID);

    String tmpdeviceChannel = String("greengame/device/") + String(deviceID); //MQTT channel for this specific device, used to post status msgs, logs, targeted OTAs, etc.
    strcpy (deviceChannel,tmpdeviceChannel.c_str());

    //set the color to red because we are disconnected
    setLEDColors(255, 0, 0, 0); // Set the color to red because we are disconnected

    client = new EspMQTTClient(
      SSID,         // TODO #1 Change to allow user to set wifi password
      WIFIPASS,
      BROKER,       // MQTT Broker server ip
      MQTTu,        // Can be omitted if not needed
      MQTTp,     // "green1" Client name that uniquely identify your device  #TODO #2 make MQTT login client name dynamic somehow
      mqttuser,  // Client name that uniquely identify your device, default to "ESP32Client" if not set
      1883          // The MQTT port, default to 1883. this line can be omitted
    );

    // Optional functionalities of EspMQTTClient
    client->enableDebuggingMessages(); // Enable debugging messages sent to serial output
    //client->enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridden with enableHTTPWebUpdater("user", "password").
    //client->enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
    client->enableLastWillMessage(deviceChannel, "Disconnected");  // You can activate the retain flag by setting the third parameter to true
    client->setKeepAlive(60); // Set the keep alive interval in seconds, default is 15 seconds

    while(!client->isWifiConnected())
    {
      client->loop();
      setLEDColors(0, 255, 0, 0); // Set the color to blue -> connected to wifi but not MQTT
      display(colors);
    }

    syncNTP();
    calcCurrentTimeMillis(); 
    printCurrentTimeMillis();

    //Zero out the system time, we will use this time to compute who the winner is on a MQTT touch event
    syncTime=millis();
    startMillis = millis();  //initial start time
    return;
  } else {
    Serial.println("No stored WiFi credentials.");
  }
  // If we get here, provisioning is needed
  startProvisioningAP();
}

void loop()
{
  unsigned long elapsed = millis() - startTime;

  // If in provisioning mode, handle incoming HTTP clients
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();  // handle captive-portal DNS
    server.handleClient();
    // Loop the transition
    if (elapsed > TRANSITION_DURATION) {
      startTime = millis();
      forward = !forward; // alternate direction
      return;
    }

    float t = (float)elapsed / TRANSITION_DURATION;
    float easedT = cubicEaseInOut(t);

    // Interpolate between blue (0, 0, 255) and yellow (255, 255, 0)
    int r = interpolate(0, 255, forward ? easedT : 1.0 - easedT);
    int g = interpolate(0, 255, forward ? easedT : 1.0 - easedT);
    int b = interpolate(255, 0, forward ? easedT : 1.0 - easedT);
    setLEDColors(r, b, g, 0); 
    display(colors);
  }
  // If not in provisioning mode, handle normal operation
  else {
    //Serial.println("Normal operation mode");
    client->loop(); //Wifi keep alive
    display(colors); //TODO putting this here so we can have a progressive fade/blink/refresh in the future

    if (client->isMqttConnected()){
      if (touchBtn.pressed) {
        deltaTime = touchBtn.touchTime - syncTime;
        if (deltaTime >= debouceTime){
          String log = String("touch Event at delta of: " + String(touchBtn.delta));
          Serial.println(log);
          client->publish(deviceChannel, log);

          setLEDColors(0, 0, 255, 0); 

          // Publish a message 
          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "touch";
          jsonTxBuffer["device"] = deviceID; 
          jsonTxBuffer["delta"] = touchBtn.delta;
          sendJSON(jsonTxBuffer, "greengame/events/"); 
        }

        touchBtn.pressed = false;
      }

      if (event.newEvent) {
        //deltaTime = event.eventTime - syncTime;

        Serial.println(touchBtn.touchTime - syncTime);
        if (event.eventTime < (touchBtn.touchTime - syncTime)){
          String log = String("they win\n Event occured at: " + String(event.eventTime) + "\n Last touch Event at: " + String(touchBtn.delta));
          Serial.println(log);
          client->publish(deviceChannel, log);

          //the event happened sooner than our last touch event, filters out delayed messages?? thats what I am telling myself.
          //set the color to red and sync the time
          setLEDColors(0, 0, 0, 255); 

          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "sync";
          jsonTxBuffer["device"] = deviceID; 
          sendJSON(jsonTxBuffer, "greengame/events/");
        }
        else if (event.eventTime >= (touchBtn.touchTime - syncTime)){
          String log = String("they lose\n Event occured at: " + String(event.eventTime) + "\n Last touch Event at: " + String(touchBtn.delta));

          Serial.println(log);
          client->publish(deviceChannel, log);

          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "sync";
          jsonTxBuffer["device"] = deviceID; 
          sendJSON(jsonTxBuffer, "greengame/events/");
        }

        display(colors);
        event.newEvent = false;    
      }
    }
    else {
      //Show magenta anytime the MQTT connection has died
      //TODO Change offline indicator to breatheing
      setLEDColors(128, 128, 0, 0); 
    }
  }
}

void IRAM_ATTR touchEvent(){
  touchBtn.touchTime = millis();
  touchBtn.delta = touchBtn.touchTime - syncTime;
  touchBtn.pressed = true;
  //Serial.println("touch event detected");
  //Serial.println(touchBtn.touchTime);
  //Serial.println(touchBtn.delta);
  //Serial.println(syncTime);
  //Serial.println(millis());
}

void synchronize(){
  syncTime = millis();
}

void recieveEvents(const String& msg){
  /*event types:
    0: No Event/Unknown
    FF: other event
    TODO 2: OTA
    3: Touch
    4: Sync
    TODO 5: Disable touch events via MQTT
    TODO 6: factory reset
  */
  StaticJsonDocument<200> jsonRxBuffer;
  DeserializationError error = deserializeJson(jsonRxBuffer, msg);
  if (error){
    Serial.println("event did not contain JSON");
    Serial.print(msg);
  }
  else if(jsonRxBuffer["event"] == "OTA"){
    //Serial.println("got MQTT OTA event");
    //serializeJson(jsonRxBuffer, Serial);
    if (jsonRxBuffer.containsKey("url")) {
      String otaUrl = jsonRxBuffer["url"].as<String>();
      Serial.println("Received OTA event, fetching firmware from: " + otaUrl);
      if (jsonRxBuffer.containsKey("persist")) {
        bool persist = jsonRxBuffer["persist"].as<bool>();
        Serial.println("Persist OTA: " + String(persist));
        fetchOTA(otaUrl, persist);
      } else {
        Serial.println("No persist flag provided, defaulting to true.");
        fetchOTA(otaUrl);
      }
    } else {
      Serial.println("OTA event received but no URL provided.");
    }
  }  
  else if(jsonRxBuffer["event"] == "touch"){
    //Serial.println(msg);
    Serial.println("got MQTT touch event");
    serializeJson(jsonRxBuffer, Serial);
    if (jsonRxBuffer["device"] != deviceID){
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
  else if(jsonRxBuffer["event"] == "reset"){ //someone just  processed a wining event - everyone clear thier timers to sync up
    factoryReset();
  } 
  else {
    Serial.println("unknown JSON event type");
    //Serial.print(jsonRxBuffer);
  }
}

void factoryReset() {
  // Reset the device to factory settings
  Serial.println("Factory reset initiated.");
  // Clear stored WiFi credentials
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  // Optionally, reset other settings or configurations here

  // Restart the device
  ESP.restart();
}

void display(const struct LEDstruct led) {
  analogWrite(REDPIN, led.redBrightness);
  analogWrite(BLUEPIN, led.blueBrightness);
  analogWrite(GREENPIN, led.greenBrightness);
  analogWrite(WHITEPIN, led.whiteBrightness);
}

void setLEDColors(uint8_t red, uint8_t blue, uint8_t green, uint8_t white) {
  colors.redBrightness = red;
  colors.blueBrightness = blue;
  colors.greenBrightness = green;
  colors.whiteBrightness = white;
  display(colors);
}

String getMacAddress(){
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);

  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  String macAddress = String(baseMacChr);

  Serial.print("MAC Address :: ");
  Serial.println(macAddress);

  return String(baseMacChr);
}

void sendJSON(const JsonDocument& json, const char* channel){
  char msg[128];
  int b =serializeJson(json, msg);
  Serial.print("message length = ");
  Serial.println(b,DEC);
  client->publish(String(channel), msg); // You can activate the retain flag by setting the third parameter to true  
}

void onConnectionEstablished(){
  // This function is called once everything is connected (Wifi and MQTT), is used to register callbacks for MQTT messages recieved
  // Subscribe to "mytopic/test" and display received message to Serial
  client->subscribe("greengame/events/", recieveEvents);
  client->subscribe("greengame/device/"+ String(deviceID), recieveEvents);
  //client->subscribe(String("greengame/OTA/" + String(deviceID)), fetchOTA);

  // Publish a message 
  StaticJsonDocument<200> jsonTxBuffer;
  jsonTxBuffer["event"] = "connected";
  jsonTxBuffer["device"] = deviceID; 
  jsonTxBuffer["ipaddr"] = WiFi.localIP();
  jsonTxBuffer["FW_Ver"] = FW_Version;
  jsonTxBuffer["HW_Ver"] = HW_Version;
  sendJSON(jsonTxBuffer, deviceChannel); 

  // Publish a message to "mytopic/test"
  client->publish(deviceChannel, "Connected"); // You can activate the retain flag by setting the third parameter to true

  setLEDColors(0, 0, 0, 255); 
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

bool fetchOTA(const String& url, bool persist) {
  bool status = false;
  // Check if the URL starts with "http"
  if (!url.startsWith("http")) {
    Serial.println("OTA URL must start with http:// or https://");
    Serial.println(url);
    return false;
  }
  // Connect to external web server
  Serial.print("Starting OTA update from URL: ");
  Serial.println(url);

  if (!persist) {
    // Clear previous wifi credentials
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
  }

  int resp = OTAclient.begin(url);
  if (resp != 1) {
    Serial.print("OTAclient.begin() failed, return code: ");
    Serial.println(resp);
    OTAclient.end();
    return false;
  }
  resp = OTAclient.GET();
  Serial.print("Response: ");
  Serial.println(resp);
  // If file is reachable, start downloading
  if(resp == 200){
    // get length of document (is -1 when Server sends no Content-Length header)
    totalLength = OTAclient.getSize();
    // transfer to local variable
    int len = totalLength;
    // this is required to start firmware update process
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Serial.println("Update.begin() failed!");
      OTAclient.end();
      return false;
    }
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
    }
    status = true;
  }else{
    Serial.println("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location.");
    status = false;
  }
  OTAclient.end();
  return status;
}

void syncNTP() {
  // Initialize and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
}  

void calcCurrentTimeMillis() {
  // Calculate the boot time in milliseconds since the epoch
  time_t now;
  time(&now);
  bootTimeMillis = now * 1000UL - millis();
}

void printCurrentTimeMillis() {
  unsigned long currentMillis = millis();
  unsigned long currentTimeMillis = bootTimeMillis + currentMillis;
  Serial.print("Current time in milliseconds since epoch: ");
  Serial.println(currentTimeMillis);
}

void removeColons(char* str) {
  int length = strlen(str);
  int j = 0;
  for (int i = 0; i < length; ++i) {
    if (str[i] != ':') {
      str[j++] = str[i];
    }
  }
  str[j] = '\0'; // Null-terminate the modified string
}

// Handle root path: serve the form
void handleRoot() {
  server.send(200, "text/html", provisioningPage);
}

// Handle form submission: save credentials then reboot
void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String deviceName = server.arg("deviceName");
  if (ssid.length() > 0 && pass.length() > 0 && deviceName.length() > 0 ) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("deviceName", deviceName);
    prefs.end();

    server.send(200, "text/html",
      "<html><body><h3>Credentials saved!</h3>"
      "<p>Rebooting...</p></body></html>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "All field are required!");
  }
}

void startProvisioningAP() {
  // Get device ID (MAC address without colons)
  String mac = getMacAddress();
  mac.replace(":", "");
  String suffix = mac.substring(mac.length() - 4); // last 4 characters
  String apName = "fungers-" + suffix;

  WiFi.softAPConfig(apIP, gateway, subnet);

  // AP SSID will be "fungers-xxxx"
  WiFi.softAP(apName.c_str());
  //ip = WiFi.softAPIP();
  Serial.printf("Provisioning AP started. Connect to http://%s (SSID: %s)\n", apIP.toString().c_str(), apName.c_str());

  dnsServer.start(53, "*", apIP); // Start DNS server to redirect all requests to the AP IP

  // Setup HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/generate_204", HTTP_GET, handleRoot); // Android
  server.on("/fwlink", HTTP_GET, handleRoot);       // Windows
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot); // Apple
  server.on("/ncsi.txt", HTTP_GET, handleRoot);     // Windows  
  server.onNotFound(handleNotFound);

  server.begin();
}

void handleNotFound() {
  // (optional) debug
  Serial.printf("NotFound: %s\n", server.uri().c_str());

  // Redirect to the portal root
  String redirectURL = String("http://") + apIP.toString() + "/";
  server.sendHeader("Location", redirectURL, true);   // true = replace any existing header
  // iOS captive-portal requires a non-empty body
  server.send(302, "text/html", provisioningPage); // or your config HTML
}

int interpolate(int start, int end, float t) {
  return start + (end - start) * t;
}

// Cubic easing in/out: accelerating then decelerating
float cubicEaseInOut(float t) {
  if (t < 0.5) {
    return 4 * t * t * t;
  } else {
    float f = (2 * t) - 2;
    return 0.5 * f * f * f + 1;
  }
}

// HSV to RGB conversion (H: 0–360, S/V: 0–1)
void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
  int i = int(h / 60.0) % 6;
  float f = (h / 60.0) - i;
  float p = v * (1.0 - s);
  float q = v * (1.0 - f * s);
  float t = v * (1.0 - (1.0 - f) * s);

  switch (i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
}
