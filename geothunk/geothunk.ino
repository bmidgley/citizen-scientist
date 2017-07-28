#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "SSD1306.h"
#include "ESP8266TrueRandom.h"
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <time.h>

// use arduino library manager to get libraries
// sketch->include library->manage libraries
// WiFiManager, ArduinoJson, PubSubClient, ArduinoOTA, "ESP8266 and ESP32 Oled Driver for SSD1306 display"
// wget https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip
// unzip master.zip
// mv ESP8266TrueRandom-master ~/Documents/Arduino/libraries/
// or
// mv ESP8266TrueRandom-master ~/Arduino/libraries/

#define MDNS_NAME "geothunk"
#define TRIGGER_PIN 0

bool shouldSaveConfig = false;
long lastMsg = 0;
long lastReading = 0;
long lastSwap = 0;
char msg[200];
int reconfigure_counter = 0;

char mqtt_server[40] = "mqtt.geothunk.com";
char mqtt_port[6] = "8080";
char uuid[64] = "";
char gps_port[10] = "";
char ota_password[10] = "";
char particle_topic_name[128];
char error_topic_name[128];
char ap_name[64];
char *version = "1.2";

unsigned int pm1 = 0;
unsigned int pm2_5 = 0;
unsigned int pm10 = 0;

int byteGPS=-1;
char linea[300] = "";
char comandoGPR[7] = "$GPRMC";
int cont=0;
int bien=0;
int conta=0;
int indices[13];
int lats = 1;
int latw = 0;
int latf = 0;
int lngs = 1;
int lngw = 0;
int lngf = 0;

WiFiClient *tcpClient;
PubSubClient *client;
ESP8266WebServer *webServer;
SSD1306 display(0x3c,5,4);

t_httpUpdate_return update() {
  return ESPhttpUpdate.update("updates.geothunk.com", 80, "/updates/geothunk", version);
}

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
 while (!client->connected()) {
 Serial.print("Attempting MQTT connection...");
 if (client->connect(uuid)) {
  Serial.println("connected");
  client->subscribe("clients");
 } else {
  Serial.print("failed, rc=");
  Serial.print(client->state());
  Serial.println(" try again in 5 seconds");
  delay(5000);
  }
 }
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  WiFiManager wifiManager;
  bool create_ota_password = true;
  byte uuidNumber[16];
  byte uuidCode[16];
  
  Serial.begin(9600);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT);
  WiFi.printDiag(Serial);
  
  display.init();
  display.setContrast(255);
  display.clear();
  
  ESP8266TrueRandom.uuid(uuidCode);
  ESP8266TrueRandom.uuidToString(uuidCode).toCharArray(ota_password, 7);

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("found /config.json");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("reading /config.json");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          if(json["mqtt_server"]) strcpy(mqtt_server, json["mqtt_server"]);
          if(json["mqtt_port"]) strcpy(mqtt_port, json["mqtt_port"]);
          if(json["uuid"]) strcpy(uuid, json["uuid"]);
          if(json["gps_port"]) strcpy(gps_port, json["gps_port"]);
          if(json["ota_password"]) {
            strcpy(ota_password, json["ota_password"]);
            create_ota_password = false;
          }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  Serial.println("loaded config");
  
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_gps_port("gps_port", "GPS server port (optional)", gps_port, 10);
  WiFiManagerParameter custom_ota_password("ota_password", "OTA password (optional)", ota_password, 6);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_gps_port);
  if(create_ota_password) {
    Serial.println("generating ota_password");
    wifiManager.addParameter(&custom_ota_password);
    saveConfigCallback();
  }

  snprintf(ap_name, 64, "Geothunk-%d", ESP8266TrueRandom.random(100, 1000));
  Serial.printf("autoconnect with AP name %s\n", ap_name);

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  if(WiFi.SSID() && WiFi.SSID() != "") {
    String status("Connecting to ");
    status.concat(WiFi.SSID());
    status.concat(" or...");
    display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 24, status);
  }
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 14, String("Connect to this wifi"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, String(ap_name));
  display.display();
  
  wifiManager.autoConnect(ap_name);
  Serial.println("stored wifi connected");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(gps_port, custom_gps_port.getValue());
  if(uuid == NULL || *uuid == 0) {
    Serial.println("generating uuid");
    ESP8266TrueRandom.uuid(uuidNumber);
    ESP8266TrueRandom.uuidToString(uuidNumber).toCharArray(uuid, 64);
    saveConfigCallback();
  }
  
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["uuid"] = uuid;
    json["gps_port"] = gps_port;
    json["ota_password"] = ota_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");
    json.printTo(configFile);
    configFile.close();
  }

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 5, String("Connecting to Server"));
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(DISPLAY_WIDTH, DISPLAY_HEIGHT - 10, String(WiFi.SSID()));
  display.display();

  client = new PubSubClient(*(new WiFiClient()));
  client->setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client->setCallback(mqttCallback);

  for (int i=0;i<300;i++) {
    linea[i]=' ';
  }
  tcpClient = new WiFiClient();
  tcpClient->connect(WiFi.gatewayIP(), atoi(gps_port));
  snprintf(particle_topic_name, 128, "%s/particles", uuid);
  snprintf(error_topic_name, 128, "%s/errors", uuid);

  Serial.printf("publishing data on %s\n", particle_topic_name);
  Serial.printf("publishing errors on %s\n", error_topic_name);

  Serial.printf("ota_password is %s\n", ota_password);

  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  MDNS.begin(MDNS_NAME);
  MDNS.addService("http", "tcp", 80);
  webServer = new ESP8266WebServer(80);
  webServer->onNotFound([]() {
    webServer->send(404, "text/plain", "File not found");
  });
  webServer->begin();

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void to_degrees(char *begin, char *end, int &whole, int &decimal) {
  char copied[20];
  float result;
  strncpy(copied, begin, 10);
  copied[end-begin] = '\0';
  float nmea = atof(copied);
  whole = (int)(nmea / 100);
  nmea -= 100 * whole;
  decimal = (int)(nmea * ( 10000.0 / 60.0));
}

int handle_gps_byte(int byteGPS) {
  linea[conta]=byteGPS;
  conta++;
  if (byteGPS==13 || conta >= 300){
   cont=0;
   bien=0;
   for (int i=1;i<7;i++){
     if (linea[i]==comandoGPR[i-1]){
       bien++;
     }
   }
   if(bien==6){
     for (int i=0;i<300;i++){
       if (linea[i]==',' && cont < 13){
         indices[cont]=i;
         cont++;
       }
       if (linea[i]=='*'){
         indices[12]=i;
         cont++;
       }
     }
     for (int i=0;i<12;i++){
       switch(i){
         case 2:
           to_degrees(linea + 1 + indices[i], linea + indices[i + 1], latw, latf);
           break;
         case 3:
           lats = linea[indices[i]+1] == 'N' ? 1 : -1;
           break;
         case 4:
           to_degrees(linea + 1 + indices[i], linea + indices[i + 1], lngw, lngf);
           break;
         case 5:
           lngs = linea[indices[i]+1] == 'E' ? 1 : -1;
           break;
       }
     }
     Serial.print(".");
   }
   conta=0;
   for (int i=0;i<300;i++){
     linea[i]=' ';
   }
  }
}

void paint_display(long now) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(DISPLAY_WIDTH, 0, String(pm2_5) + String("/") + String(pm1) + String("/") + String(pm10));
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  if(now < 24 * 60 * 60 * 1000)
    display.drawString(0, 0, String(now / (60 * 60 * 1000)) + String("h ") + String(version));
  else
    display.drawString(0, 0, String(now / (24 * 60 * 60 * 1000)) + String("d ") + String(version));
  display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(DISPLAY_WIDTH, DISPLAY_HEIGHT - 10, String(WiFi.SSID()));
  display.display();
}

void handleGPS() {
  if(tcpClient->connected() && tcpClient->available()) handle_gps_byte(tcpClient->read());
}

void loop() {
  int index = 0;
  char value;
  char previousValue;

  handleGPS();
  ArduinoOTA.handle();
  webServer->handleClient();
  client->loop();

  long now = millis();
  if (now - lastMsg < 5000) {
    return;
  }
  lastMsg = now;
  
  time_t clocktime = time(nullptr);
  Serial.println(ctime(&clocktime));

  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    if(reconfigure_counter > 0) {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.setFont(ArialMT_Plain_10);
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 10, String("Hold to clear settings"));
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, String(3-reconfigure_counter));
      display.display();
    }

    reconfigure_counter++;
    if(reconfigure_counter > 2) {
      Serial.println("disconnecting from wifi to reconfigure");
      WiFi.disconnect(true);

      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.setFont(ArialMT_Plain_10);
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 10, String("Release and tap reset"));
      display.display();
    }
    return;
  } else {
    reconfigure_counter = 0;
  }

  while (Serial.available()) {
    value = Serial.read();
    if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4d)){
      break;
    }

    if (index == 4 || index == 6 || index == 8 || index == 10 || index == 12 || index == 14) {
      previousValue = value;
    }
    else if (index == 5) {
      pm1 = 256 * previousValue + value;
    }
    else if (index == 7) {
      pm2_5 = 256 * previousValue + value;
    }
    else if (index == 9) {
      pm10 = 256 * previousValue + value;
      lastReading = millis();
    } else if (index > 15) {
      break;
    }
    index++;
  }
  while(Serial.available()) {
    Serial.read();
  }
  
  if(!tcpClient->connected() && atoi(gps_port) > 0) tcpClient->connect(WiFi.gatewayIP(), atoi(gps_port));

  snprintf(msg, 200, "{\"pm1\":%u, \"pm2_5\":%u, \"pm10\":%u, \"lat\": %s%d.%d, \"lng\": %s%d.%d, \"timestamp\": %u}",
    pm1, pm2_5, pm10, lats > 0 ? "":"-", latw, latf, lngs > 0 ? "":"-", lngw, lngf, lastReading);

  if (!client->connected()) {
    mqttReconnect();
  }

  client->publish(particle_topic_name, msg);

  Serial.println("");
  Serial.println(msg);

  paint_display(now);

  if(lastMsg - lastReading > 30000) {
    snprintf(msg, 200, "{\"lastMsg\": %u, \"lastReading\": %u}", lastMsg, lastReading);
    client->publish(error_topic_name, msg);
    Serial.println(msg);
    if(now - lastSwap > 60000) {
      Serial.println("swapping from here");
      Serial.flush();
      Serial.swap();
      Serial.println("swapped to here");
      lastSwap = now;
    }
  }
}
