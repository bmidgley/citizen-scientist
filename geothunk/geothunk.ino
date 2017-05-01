#include <FS.h>

#include <ESP8266WiFi.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>

// arduino library manager to get wifimanager and arduinojson
// sketch->include library->manage libraries
// WiFiManager, ArduinoJson, PubSubClient

#define TRIGGER_PIN 0
bool shouldSaveConfig = false;
long lastMsg = 0;
char msg[75];

char mqtt_server[40] = "flamebot.com";
char mqtt_port[6] = "8080";
char blynk_token[34] = "3e82307c0ad9495d900b4e5454a3e957";
uint8_t mac[6];

WiFiClient espClient;
PubSubClient client(espClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
 for (int i=0;i<length;i++) {
  char receivedChar = (char)payload[i];
  Serial.print(receivedChar);
 }
 Serial.println();
}

void mqttReconnect() {
 while (!client.connected()) {
 Serial.print("Attempting MQTT connection...");
 if (client.connect("ESP8266 Client")) {
  Serial.println("connected");
  client.subscribe("lightening");
 } else {
  Serial.print("failed, rc=");
  Serial.print(client.state());
  Serial.println(" try again in 5 seconds");
  delay(5000);
  }
 }
}

void reset() {
  Serial.println("resetting...");
  ESP.reset();
  delay(5000);
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  WiFiManager wifiManager;

  Serial.begin(115200);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT);
  WiFi.printDiag(Serial);
  
  WiFi.macAddress(mac);
  
  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
  
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);
  
  wifiManager.autoConnect("GeothunkAP");
  Serial.println("stored wifi connected");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  client.setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client.setCallback(mqttCallback);
}

void loop() {
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    Serial.println("reconfiguring!");
    WiFi.disconnect(true);
  }
  
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    int value = analogRead(0);
    snprintf (msg, 75, "%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("lightTopic", msg);
  }
}
