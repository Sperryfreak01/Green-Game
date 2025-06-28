#define ARDUINOJSON_USE_LONG_LONG 1
//#define DEBUG_ESP_DNS_SERVER 1

#include <GreenGame.h>
#include <vector>
#include <algorithm>
// In setup(), after creating the client, register the callback:
  // client->setOnConnectionFailed(onConnectionFailed);


//===================================== Variable  Def ============================================
// Global variables
int totalLength;       //total size of firmware
int currentLength = 0; //current size of written firmware


// Duration of one full transition in milliseconds
const unsigned long TRANSITION_DURATION = 3000;

unsigned long startTime = 0;
bool forward = true;
//=================================== End Variable Def ==========================================

/**
 * @brief Initializes hardware and software components for the Fungers device.
 *
 * This function performs the following setup tasks:
 * - Records the initial start time.
 * - Configures all LED control pins as outputs and sets them LOW.
 * - Sets up an interrupt on pin 4 for capacitive touch sensing.
 * - Initializes serial communication at 115200 baud.
 * - Attempts to read stored WiFi credentials and device name from preferences.
 *   - (For testing, preferences are cleared; remove this in production.)
 * - If credentials are found:
 *   - Prints the SSID and attempts to connect to WiFi.
 *   - Retrieves and formats the device MAC address for MQTT identification.
 *   - Sets up MQTT client with device-specific parameters.
 *   - Enables MQTT debugging messages and configures keep-alive.
 *   - Waits for WiFi connection, updating LED color to indicate status.
 *   - Synchronizes time via NTP and initializes system timers.
 * - If credentials are not found:
 *   - Prints a message and starts WiFi provisioning in AP mode.
 *
 * @note Some sections are marked as TODO for future improvements (e.g., moving preference clearing to MQTT command).
 */
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
  //factoryReset(); //TODO #5 remove this in production, this is for testing purposes only, it clears the preferences
  prefs.begin("wifi", false);
  //prefs.putString("ssid", "fringeclass");
  //prefs.clear();
  prefs.end();
  
  
  // Try to read stored credentials
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  sendLog("SSID: " + ssid, DEBUG);
  pass = prefs.getString("pass", "");
  sendLog("PASS: " + pass, DEBUG);
  deviceName = prefs.getString("deviceName", "");
  sendLog("USERNAME: " + deviceName, DEBUG);

  prefs.end();
  
  if (ssid.length() > 0 && pass.length() > 0) {
    sendLog("Found saved SSID '" + ssid + "', attempting to connect...", DEBUG);
    
    //Retrieve and build the MAC string so we can use it later as a MQTT device identifier
    String tmpMAC = getMacAddress();
    strcpy(deviceID, tmpMAC.c_str());
    removeColons(deviceID);

    //Build the MQTT channel for this specific device, used to post status msgs, logs, targeted OTAs, etc.
    String tmpdeviceChannel = String("funger/device/") + String(deviceID); 
    strcpy (deviceChannel,tmpdeviceChannel.c_str());

    client = new EspMQTTClient(
      ssid.c_str(),         // TODO #1 Change to allow user to set wifi password
      pass.c_str(),
      BROKER,       // MQTT Broker server ip
      MQTTu,        // Can be omitted if not needed
      MQTTp,     // "green1" Client name that uniquely identify your device  #TODO #2 make MQTT login client name dynamic somehow
      deviceName.c_str(),  // Client name that uniquely identify your device, default to "ESP32Client" if not set
      1883          // The MQTT port, default to 1883. this line can be omitted
    );

    // Optional functionalities of EspMQTTClient
    //client->enableDebuggingMessages(); // Enable debugging messages sent to serial output
    client->enableLastWillMessage(deviceChannel, "{\"event\":\"Disconnected\"");  // You can activate the retain flag by setting the third parameter to true
    client->setKeepAlive(15); // Set the keep alive interval in seconds, default is 15 seconds

    unsigned long elapsed = millis() - startTime;    

    while(!client->isWifiConnected()){
      client->loop(); // Keep the MQTT client loop running to maintain WiFi connection
      if(client->wifiConnectFailed == true) {
        //factoryReset(); // Exit if WiFi connection failed
          startProvisioningAP();

      }
      if (elapsed > 1000) {
        startTime = millis();
        forward = !forward; // alternate direction
        return;
        }

      float t = (float)elapsed / 1000;
      float easedT = cubicEaseInOut(t);

      // Interpolate between blue (0, 0, 255) and dim blue (0, 0, 64)
      delay(1); // Small delay to allow for smoother transitions
      int b = interpolate(255, 64, forward ? easedT : 1.0 - easedT);
      setLEDColors(0, b, 0, 0);
      //setLEDColors(0, 255, 0, 0); // Set the color to blue -> not connected to WiFi
      //display(colors);

    }
    startTime = 0; // Reset start time after connection
    sendLog("Connected to WiFi: " + ssid, INFO);
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
  auto networks = scanNetworks();
  for (const auto& net : networks) {
    options += "<option value=\"" + net.ssid + "\">" + net.ssid + " (" + String(net.rssi) + " dBm)</option>";
  }
  sendLog(options, VERBOSE);
  startProvisioningAP();
}

void loop()
{

  // If in provisioning mode, handle incoming HTTP clients
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();  // handle captive-portal DNS
    server.handleClient();

    unsigned long elapsed = millis() - startTime;
    // Loop the transition
    if (elapsed > TRANSITION_DURATION) {
      startTime = millis();
      forward = !forward; // alternate direction
      elapsed = 0;  
      //return;
    }

    float t = (float)elapsed / TRANSITION_DURATION;
    float easedT = cubicEaseInOut(t);
    delay(1); // Small delay to allow for smoother transitions
    // Interpolate between blue (0, 0, 255) and yellow (255, 255, 0)
    int r = interpolate(0, 255, forward ? easedT : 1.0 - easedT);
    int g = interpolate(0, 200, forward ? easedT : 1.0 - easedT);
    int b = interpolate(255, 0, forward ? easedT : 1.0 - easedT);
    setLEDColors(r, b, g, 0); 
    //display(colors);
  }
  // If not in provisioning mode, handle normal operation
  else {
    //Serial.println("Normal operation mode");
    client->loop(); //Wifi keep alive
    display(colors); //TODO putting this here so we can have a progressive fade/blink/refresh in the future
    
    if (client->isMqttConnected()){
      if (touchBtn.pressed) { //TODO #4 add support for press and hold events to trigger clearing of Wifi settings
        deltaTime = touchBtn.touchTime - syncTime;
        if (deltaTime >= debouceTime){
          sendLog(String("touch Event at delta of: " + String(touchBtn.delta)), DEBUG);
          //set the color to green, this is the color we transition to when a touch event is detected
          //TODO #3 make the color transition to green when a touch event is detected
          setLEDColors(0, 0, 255, 0); 

          // Publish the events for other devices to see
          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "touch";
          jsonTxBuffer["device"] = deviceID; 
          jsonTxBuffer["delta"] = touchBtn.delta;
          time_t now;
          time(&now);
          jsonTxBuffer["time"] = now; //send the timestamp of the touch event
          sendJSON(jsonTxBuffer, "funger/events/"); 
        }

        touchBtn.pressed = false;
      }

      if (event.newEvent) {
        //deltaTime = event.eventTime - syncTime;

        Serial.println(touchBtn.touchTime - syncTime);
        if (event.eventTime < (touchBtn.touchTime - syncTime)){
          sendLog(String("they win\n Event occured at: " + String(event.eventTime) + "\n Last touch Event at: " + String(touchBtn.delta)), DEBUG);
          

          //the event happened sooner than our last touch event, filters out delayed messages?? thats what I am telling myself.
          //set the color to red and sync the time
          setLEDColors(0, 0, 0, 255); 

          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "sync";
          jsonTxBuffer["device"] = deviceID; 
          sendJSON(jsonTxBuffer, "funger/events/");
        }
        else if (event.eventTime >= (touchBtn.touchTime - syncTime)){
          sendLog(String("they lose\n Event occured at: " + String(event.eventTime) + "\n Last touch Event at: " + String(touchBtn.delta)), DEBUG);

          StaticJsonDocument<200> jsonTxBuffer;
          jsonTxBuffer["event"] = "sync";
          jsonTxBuffer["device"] = deviceID; 
          sendJSON(jsonTxBuffer, "funger/events/");
        }

        display(colors);
        event.newEvent = false;    
      }
    }
  
    else if(!client->isMqttConnected()){
      //Show magenta anytime the MQTT connection has died
      //TODO Change offline indicator to breatheing
      unsigned long elapsed = millis() - startTime;
      // Loop the transition
      if (elapsed > 125) {
        startTime = millis();
        forward = !forward; // alternate direction
        //return;
      }

      float t = (float)elapsed / 125;
      float easedT = cubicEaseInOut(t);

      // Interpolate between magenta (255, 0, 255) and dim magenta (127, 0, 127)
      int r = interpolate(255, 127, forward ? easedT : 1.0 - easedT);
      int g = interpolate(0, 200, forward ? easedT : 1.0 - easedT);
      int b = interpolate(255, 127, forward ? easedT : 1.0 - easedT);
      setLEDColors(r, b, g, 0); 
      //setLEDColors(128, 128, 0, 0); 
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
  void syncNTP();
  // Set the syncTime to the current millis, this will be used to calculate the delta
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
  StaticJsonDocument<300> jsonRxBuffer;
  DeserializationError error = deserializeJson(jsonRxBuffer, msg);
  if (error){
    sendLog("event did not contain JSON: " + msg);
  }
  else if(jsonRxBuffer["event"] == "OTA"){
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
        sendLog("No persist flag provided, defaulting to true.");
        fetchOTA(otaUrl);
      }
    } else {
      sendLog("OTA event received but no URL provided.");
    }
  }  
  else if(jsonRxBuffer["event"] == "connected"){
    return; //ignore connected events, we already know we are connected
  }
  else if(jsonRxBuffer["event"] == "touch"){
    //Serial.println(msg);
    sendLog("got MQTT touch event");
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
    sendLog("syncing time",DEBUG);   //will fire off everytime a player processes, this will not scale and will pump traffic
    synchronize();
    //if(jsonRxBuffer["device"] != deviceID){} //no need to sync on our own event only others...wait maybe we do so everyone has round trip latency...test it...
    //  synchronize()
    //}
  } 
  else if(jsonRxBuffer["event"] == "reset"){ //someone just  processed a wining event - everyone clear thier timers to sync up
    factoryReset();
  } 
  else {
    sendLog("unknown JSON event type"), WARN;
    //Serial.print(jsonRxBuffer);
  }
}

void factoryReset() {
  // Reset the device to factory settings
  sendLog("Factory reset initiated.",INFO);
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
    time_t now;
    time(&now);
    //unsigned long ms = now;
    sendLog("Epoch: " + String(now), VERBOSE); // Adjust for your timezone if needed

    //unsigned long s  = now / 1000UL;            // convert to seconds
    unsigned long secsToday = now % 86400UL;     // seconds since last UTC midnight
    int hour = secsToday / 3600UL;             // integer 0–23
    sendLog("Hour (UTC): " + String(hour), VERBOSE); // Adjust for your timezone if needed
    // Adjust hour for your timezone, e.g., PDT is UTC-7
    if(hour - 7 < 0) hour += 24; // Adjust for your timezone if needed
    sendLog("Hour (PDT): " + String(hour - 7), VERBOSE); // Adjust for your timezone if needed
    

  if (hour >= 7 && hour < 20) {
    // Daytime: Set colors to bright
    colors.redBrightness = red;
    colors.blueBrightness = blue;
    colors.greenBrightness = green;
    colors.whiteBrightness = white;
  } else {
    // Nighttime: Dim the colors
    colors.redBrightness = red / 2;
    colors.blueBrightness = blue / 2;
    colors.greenBrightness = green / 2;
    colors.whiteBrightness = white / 2;
  }
  display(colors);
  }

String getMacAddress(){
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);

  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  String macAddress = String(baseMacChr);

  sendLog("MAC Address :: " + macAddress, DEBUG); // Log the MAC address with level 1
  return String(baseMacChr);
}

void sendJSON(const JsonDocument& json, const char* channel){
  char msg[255];
  int msgLen =serializeJson(json, msg);
  sendLog("message length = " + String(msgLen, DEC), VERBOSE);
  client->publish(String(channel), msg); // You can activate the retain flag by setting the third parameter to true  
}

void onConnectionEstablished(){
  // This function is called once everything is connected (Wifi and MQTT), is used to register callbacks for MQTT messages recieved
  // Subscribe to "mytopic/test" and display received message to Serial
  client->subscribe("funger/events/", recieveEvents);
  client->subscribe("funger/device/"+ String(deviceID), recieveEvents);
  //client->subscribe(String("funger/OTA/" + String(deviceID)), fetchOTA);

  // Publish a message 
  StaticJsonDocument<200> jsonTxBuffer;
  jsonTxBuffer["event"] = "connected";
  jsonTxBuffer["device"] = deviceID; 
  jsonTxBuffer["userName"] = deviceName;
  jsonTxBuffer["ipaddr"] = WiFi.localIP();
  jsonTxBuffer["FW_Ver"] = FW_Version;
  jsonTxBuffer["HW_Ver"] = HW_Version;
  sendJSON(jsonTxBuffer, deviceChannel); 

  // Publish a message to "mytopic/test"
  // client->publish(deviceChannel, "Connected"); // You can activate the retain flag by setting the third parameter to true

  setLEDColors(0, 0, 0, 255); 
}

void updateFirmware(uint8_t *data, size_t len){
  // Function to update firmware incrementally
  // Buffer is declared to be 128 so chunks of 128 bytes
  // from firmware is written to device until server closes
  setLEDColors(127, 0, 0, 0); // Set the color to blue -> connected to wifi but not MQTT
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

bool fetchOTA(const String& url, bool persist) { //#TODO #3 add status reporting over MQTT
  bool status = false;
  String log;

  // Check if the URL starts with "http"
  if (!url.startsWith("http")) {
    sendLog("OTA URL must start with http:// or https:// recieved: " + url, ERROR);
    return false;
  }
  // Connect to external web server
  sendLog("Starting OTA update from URL: " + url, INFO);


  if (!persist) {
    // Clear previous wifi credentials
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
  sendLog("OTA file response: "+ String(resp), DEBUG);
  // If file is reachable, start downloading
  if(resp == 200){
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
    while(OTAclient.connected() && (len > 0 || len == -1)) {
      setLEDColors(255, 0, 0, 0); // Set the color to blue -> connected to wifi but not MQTT
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
  }else{
    sendLog("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location.", WARN);
    status = false;
  }
  OTAclient.end();
  return status;
}

void sendLog(const String& log, int msgLevel) {
  
  if (msgLevel <= logLevelSerial){
    // Log to Serial if the log level is less than or equal to the set log level
    Serial.println(log);
  }
    if (msgLevel <= logLevelMQTT && client->isMqttConnected()) {
    // Publish a message 
    StaticJsonDocument<200> jsonTxBuffer;
    jsonTxBuffer["level"] = msgLevel;
    jsonTxBuffer["entry"] = log; 
    time_t now;
    time(&now);
    jsonTxBuffer["time"] = now; //send the time of the log entry
    sendJSON(jsonTxBuffer, ("funger/device/" + String(deviceID) + "/logs").c_str()); 
  }
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
  unsigned long currentTimeMillis = (bootTimeMillis + currentMillis);
  sendLog("Current time in milliseconds since epoch: "+ String(currentTimeMillis), DEBUG);
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
String page = String(R"rawliteral(
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
  <img class="center" src="...your image..." />
  <h2 class="logo-text">Configure WiFi</h2>
  <div class="card">
    <form action="/save" method="POST">
      <div class="mb-1 form-group">
        <label for="ssid">SSID</label>
        <select id="ssid" name="ssid" class="p-1">
          <option value="">Select WiFi network</option>
          )rawliteral" + options + R"rawliteral(
        </select>
        <input id="ssid_custom" name="ssid_custom" type="text" class="p-1" placeholder="Or enter SSID manually" />
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
  <script>
    // If user selects from dropdown, update the text input
    document.getElementById('ssid').addEventListener('change', function() {
      document.getElementById('ssid_custom').value = this.value;
    });
  </script>
</body>
</html>
  )rawliteral");
  sendLog(page, VERBOSE);
  server.send(200, "text/html", page);
  //server.send(200, "text/html", provisioningPage);
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

    server.send(200, psavePage);
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "All field are required!");
  }
}

std::vector<NetworkInfo> scanNetworks() {
  WiFi.mode(WIFI_STA); // Ensure we're in station mode
  delay(100);          // Allow mode switch to settle
  std::vector<NetworkInfo> networks;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    networks.push_back({WiFi.SSID(i), WiFi.RSSI(i)});
  }
  // Sort by RSSI (signal strength), descending
  std::sort(networks.begin(), networks.end(), [](const NetworkInfo& a, const NetworkInfo& b) {
    return a.rssi > b.rssi;
  });
  // Remove duplicates (same SSID)
  std::vector<NetworkInfo> uniqueNetworks;
  for (const auto& net : networks) {
    bool exists = false;
    for (const auto& u : uniqueNetworks) {
      if (u.ssid == net.ssid) {
        exists = true;
        break;
      }
    }
    if (!exists && net.ssid.length() > 0) uniqueNetworks.push_back(net);
    if (uniqueNetworks.size() >= 10) break;
  }
  return uniqueNetworks;
}

void startProvisioningAP() {
    WiFi.mode(WIFI_AP); // Set WiFi mode to Access Point
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

    // Setup HTTP routes required to host the captive portal
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    // Handle common captive portal URLs
    server.on("/generate_204", HTTP_GET, handleRoot); // Android
    server.on("/fwlink", HTTP_GET, handleRoot);       // Windows
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot); // Apple
    server.on("/ncsi.txt", HTTP_GET, handleRoot);     // Windows  
    // Catch-all for any other requests
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