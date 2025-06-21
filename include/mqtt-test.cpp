#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "fringeclass";          // Replace with your Wi-Fi SSID
const char* password = "reasonwillprevail";  // Replace with your Wi-Fi password

// MQTT broker settings
const char* mqtt_server = "mattlovett.com";
const int mqtt_port = 1883;
const char* mqtt_user = "kb2G6kXGEfyErkMpCWG7";     // Set if needed, else leave empty
const char* mqtt_pass = "kb2G6kXGEfyErkMpCWG7";     // Set if needed, else leave empty

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("test/topic", "Hello from Wemos D1 ESP32!");
      // ... and resubscribe
      client.subscribe("test/topic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}



