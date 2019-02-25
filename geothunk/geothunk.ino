//#define SPI_DISPLAY
//#define SH1106_DISPLAY
//#define NO_AUTO_SWAP
//#define DEBUG
#ifdef DEBUG
#define NO_AUTO_SWAP
#endif

#ifdef SPI_DISPLAY
#define NO_AUTO_SWAP
#endif

#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "SSD1306Spi.h"
#include "SH1106.h"
#include "SSD1306.h"
#include "ESP8266TrueRandom.h"
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SimpleDHT.h>
#include <Servo.h>
#include "PmsSensorReader.h"
#include "Presentation.h"

int pinDHT = D3;
#ifdef DHT22
SimpleDHT22 dht(pinDHT);
#else
SimpleDHT11 dht(pinDHT);
#endif

// use arduino library manager to get libraries
// sketch->include library->manage libraries
// WiFiManager, ArduinoJson, PubSubClient, ArduinoOTA, SimpleDHT, "ESP8266 and ESP32 Oled Driver for SSD1306 display"
// wget https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip
// sketch->include library->Add .zip Library
// or... manually...
// unzip master.zip
// mv ESP8266TrueRandom-master ~/Documents/Arduino/libraries/
// or
// mv ESP8266TrueRandom-master ~/Arduino/libraries/

#define DEFAULT_MDNS_NAME "geothunk"
#define TRIGGER_PIN 0
#define LED_PIN D4
#define PULSE_PIN D8

bool shouldSaveConfig = false;
long lastSample = 0;
long lastReport = 0;
long lastPmReading = 0;
long lastDHTReading = 0;
long lastSwap = 0;
long nextFrame = 0; // Time at which the next frame should be drawn
char msg[200] = "";
char errorMsg[200] = "";
int disconnects = 0;

char mqtt_server[40] = "mqtt.geothunk.com";
char mqtt_port[6] = "8080";
char uuid[64] = "";
char gps_port[10] = "";
char ota_password[10] = "";
char mdns_name[40] = DEFAULT_MDNS_NAME;

char particle_topic_name[128];
char error_topic_name[128];
char ap_name[64] = "";

int sampleGap = 4 * 1000;
int reportGap = 60 * 1000;
int reportExpectedDurationMs = sampleGap * 5;
int byteGPS = -1;
char linea[300] = "";
char comandoGPR[] = "$GPRMC";
int cont = 0;
int bien = 0;
int conta = 0;
int indices[13];
int lats = 1;
int latw = 0;
int latf = 0;
int lngs = 1;
int lngw = 0;
int lngf = 0;

WiFiClientSecure *tcpClient;
PubSubClient *client;
ESP8266WebServer *webServer;
PmsSensorReader pmsSensor;

#ifdef SPI_DISPLAY
SSD1306Spi display(D8, D1, D2); // rst n/c, dc D1, cs D2, clk D5, mosi/di/si D7
#else
#ifdef SH1106_DISPLAY
SH1106 display(0x3c, D1, D2);
#else
SSD1306 display(0x3c, 5, 4);
#endif
#endif

AirData airData;
Presentation presentation(&display, &airData);
Servo myservo;

const char* serverIndex = "<html><head><script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/smoothie/1.34.0/smoothie.js'></script> <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.js'></script> <script type='text/Javascript'> \
var sc = new SmoothieChart({ responsive: true, millisPerPixel: 1000, labels: { fontSize: 30, precision: 0 }, grid: { fillStyle: '#6699ff', millisPerLine: 100000, verticalSections: 10 }, yRangeFunction: function(r) { return { min: 0, max: Math.max(30, 10*Math.ceil(0.09 * r.max)) } } }); \
var line1 = new TimeSeries(); var line2 = new TimeSeries(); var line3 = new TimeSeries(); \
sc.addTimeSeries(line1, { strokeStyle:'rgb(180, 50, 0)', fillStyle:'rgba(180, 50, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line2, { strokeStyle:'rgb(255, 0, 0)', fillStyle:'rgba(255, 0, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line3, { strokeStyle:'rgb(255, 50, 0)', fillStyle:'rgba(255, 50, 0, 0.4)', lineWidth:3 }); \
$(document).ready(function() { sc.streamTo(document.getElementById('graphcanvas1')); }); \
setInterval(function() { $.getJSON('/stats',function(data){ line1.append(Date.now(), data.pm2); line2.append(Date.now(), data.pm1); if(data.pm10 < 3*data.pm2) line3.append(Date.now(), data.pm10); }); }, 2000); \
</script></head><body>  <canvas id='graphcanvas1' style='width:100%%; height:75%%;'></canvas><p>Register device <a href='https://geothunk.com/devices?d=%s'>online</a>.</p></body></html>";

t_httpUpdate_return update() {
  return ESPhttpUpdate.update("http://updates.geothunk.com/updates/geothunk-" VERSION ".ino.bin");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
#ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
  }
  Serial.println();
#endif
}

int mqttConnect() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }

  if (client->connected()) {
    disconnects = 0;
    return 1;
  }

  if (client->connect(uuid)) {
    client->subscribe("clients");
    return 1;
  } else {
    disconnects += 1;
    return 0;
  }
}

void saveConfigCallback () {
  shouldSaveConfig = true;
}

void to_degrees(char *begin, char *end, int &whole, int &decimal) {
  char copied[20];
  float result;
  strncpy(copied, begin, 10);
  copied[end - begin] = '\0';
  float nmea = atof(copied);
  whole = (int)(nmea / 100);
  nmea -= 100 * whole;
  decimal = (int)(nmea * ( 10000.0 / 60.0));
}

int handle_gps_byte(char byteGPS) {
  linea[conta] = byteGPS;
  conta++;
  if (byteGPS == 13 || conta >= 300) {
    cont = 0;
    bien = 0;
    for (int i = 1; i < 7; i++) {
      if (linea[i] == comandoGPR[i - 1]) {
        bien++;
      }
    }
    if (bien == 6) {
      for (int i = 0; i < 300; i++) {
        if (linea[i] == ',' && cont < 13) {
          indices[cont] = i;
          cont++;
        }
        if (linea[i] == '*') {
          indices[12] = i;
          cont++;
        }
      }
      for (int i = 0; i < 12; i++) {
        switch (i) {
          case 2:
            to_degrees(linea + 1 + indices[i], linea + indices[i + 1], latw, latf);
            break;
          case 3:
            lats = linea[indices[i] + 1] == 'N' ? 1 : -1;
            break;
          case 4:
            to_degrees(linea + 1 + indices[i], linea + indices[i + 1], lngw, lngf);
            break;
          case 5:
            lngs = linea[indices[i] + 1] == 'E' ? 1 : -1;
            break;
        }
      }
    }
    conta = 0;
    for (int i = 0; i < 300; i++) {
      linea[i] = ' ';
    }
  }
}

bool pmOverdue(long now) {
  return (now - lastPmReading) > reportExpectedDurationMs;
}

bool dhtOverdue(long now) {
  return (now - lastDHTReading) > reportExpectedDurationMs;
}



void handleGPS() {
  if (tcpClient->connected() && tcpClient->available()) handle_gps_byte(tcpClient->read());
}

void measureDHT() {
  int err = SimpleDHTErrSuccess;

  float temperature, humidity = 0;
  err = dht.read2(&temperature, &humidity, NULL);
#ifdef DEBUG
  if(err != SimpleDHTErrSuccess) {
    Serial.printf("Read DHTxx failed t=%d err=%02x\n", err >> 8, err & 0xff);
  } else {
    Serial.print("Sample OK: ");
    Serial.print((float)temperature); Serial.print(" *C, ");
    Serial.print((float)humidity); Serial.println(" RH%");
    Serial.println();
  }
#endif
  if (err == SimpleDHTErrSuccess) {
    lastDHTReading = millis();
    airData.humidity = humidity;
    airData.temperature = temperature;
    airData.tempHumidityStatus = AirData_Ok;
  }

  presentation.recordGraphTemperature();
}

void setup_apStartedCallback(WiFiManager* wifiManager) {
  presentation.paintServingAp(String(ap_name));
}

void setup() {
  WiFiManager wifiManager;
  byte uuidNumber[16];
  byte uuidCode[16];

  airData.pmStatus = AirData_Uninitialized;
  airData.tempHumidityStatus = AirData_Uninitialized;

  Serial.begin(9600);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PULSE_PIN, OUTPUT);
  tone(PULSE_PIN, 40);
  WiFi.printDiag(Serial);
  myservo.attach(D0);

  display.init();
#ifndef SPI_DISPLAY
  display.flipScreenVertically();
#endif
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
#ifdef DEBUG
          Serial.println("\nparsed json");
#endif

          if (json.is<char*>("mqtt_server")) strncpy(mqtt_server, json["mqtt_server"].as<char*>(), sizeof(mqtt_server));
          if (json.is<char*>("mqtt_port")) strncpy(mqtt_port, json["mqtt_port"].as<char*>(), sizeof(mqtt_port));
          if (json.is<char*>("mdns_name")) strncpy(mdns_name, json["mdns_name"].as<char*>(), sizeof(mdns_name));
          if (json.is<char*>("gps_port")) strncpy(gps_port, json["gps_port"].as<char*>(), sizeof(gps_port));
          if (json.is<char*>("ota_password")) strncpy(ota_password, json["ota_password"].as<char*>(), sizeof(ota_password));
          if (json.is<char*>("uuid")) strncpy(uuid, json["uuid"].as<char*>(), sizeof(uuid));
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  Serial.println("\nloaded config");

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, sizeof(mqtt_server));
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, sizeof(mqtt_port));
  WiFiManagerParameter custom_mdns_name("name", "Hostname (optional)", mdns_name, sizeof(mdns_name));
  WiFiManagerParameter custom_gps_port("gps_port", "GPS server port (optional)", gps_port, sizeof(gps_port));
  WiFiManagerParameter custom_ota_password("ota_password", "OTA password (optional)", ota_password, sizeof(ota_password));

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mdns_name);
  wifiManager.addParameter(&custom_gps_port);
  if (*ota_password == 0) {
    Serial.println("generating ota_password");
    wifiManager.addParameter(&custom_ota_password);
    saveConfigCallback();
  }
  if (*uuid == 0) {
    Serial.println("generating uuid");
    ESP8266TrueRandom.uuid(uuidNumber);
    ESP8266TrueRandom.uuidToString(uuidNumber).toCharArray(uuid, 64);
    saveConfigCallback();
  }

  snprintf(ap_name, sizeof(ap_name), "Geothunk-%d", ESP8266TrueRandom.random(100, 1000));
  Serial.printf("autoconnect with AP name %s\n", ap_name);

#ifndef NO_AUTO_SWAP
  Serial.swap();
#endif

  wifiManager.setTimeout(600);
  wifiManager.setAPCallback(setup_apStartedCallback);

  presentation.paintConnectingWifi();

  do {
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      WiFi.disconnect(true);
    }

    measureDHT();

  } while (!wifiManager.autoConnect(ap_name));

  Serial.println("stored wifi connected");

  WiFi.setAutoConnect(true);

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strncpy(mdns_name, custom_mdns_name.getValue(), sizeof(mdns_name));
  strcpy(gps_port, custom_gps_port.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mdns_name"] = mdns_name;
    json["gps_port"] = gps_port;
    json["ota_password"] = ota_password;
    json["uuid"] = uuid;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");
    json.printTo(configFile);
    configFile.close();
  }

  presentation.paintConnectingMqtt();

  client = new PubSubClient(*(new WiFiClient()));
  client->setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client->setCallback(mqttCallback);

  for (int i = 0; i < 300; i++) {
    linea[i] = ' ';
  }
  tcpClient = new WiFiClientSecure();
  tcpClient->connect(WiFi.gatewayIP(), atoi(gps_port));
  snprintf(particle_topic_name, sizeof(particle_topic_name), "%s/particles", uuid);
  snprintf(error_topic_name, sizeof(error_topic_name), "%s/errors", uuid);

  Serial.printf("publishing data on %s\n", particle_topic_name);
  Serial.printf("publishing errors on %s\n", error_topic_name);

  Serial.printf("ota_password is %s\n", ota_password);
#ifndef DEBUG
  ArduinoOTA.setPassword(ota_password);
#endif

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

  MDNS.begin(mdns_name);
  MDNS.addService("http", "tcp", 80);
  webServer = new ESP8266WebServer(80);
  webServer->onNotFound([]() {
    webServer->send(404, "text/plain", "File not found");
  });
  webServer->on("/", HTTP_GET, []() {
    char response[1600];
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    snprintf(response, sizeof(response), serverIndex, uuid);
    webServer->send(200, "text/html", response);
  });
  webServer->on("/stats", HTTP_GET, []() {
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    webServer->send(200, "application/json", msg);
  });
  webServer->begin();

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void pmsSensorLoop(long now) {
#ifdef DEBUG
  if (!Serial.available())
    return;

  Serial.printf("serial: ");
#endif

  while(Serial.available()) {
    unsigned char value = Serial.read();
#ifdef DEBUG
    Serial.printf("%x ", value);
#endif
    if (pmsSensor.offer(value)) {
      lastPmReading = now;
      airData.pm1 = pmsSensor.pm1;
      airData.pm2_5 = pmsSensor.pm2_5;
      airData.pm10 = pmsSensor.pm10;
      airData.pmStatus = AirData_Ok;
      presentation.recordGraphPm2();
    }
  }

#ifdef DEBUG
  Serial.printf("\n");
#endif
}

void loop() {
  handleGPS();
  ArduinoOTA.handle();
  webServer->handleClient();
  client->loop();

  long now = millis();
  pmsSensorLoop(now);

  if (now >= nextFrame) {
    if (dhtOverdue(now)) {
      airData.tempHumidityStatus = AirData_Stale;
    } else {
      airData.tempHumidityStatus = AirData_Ok;
    }

    if (pmOverdue(now)) {
      airData.pmStatus = AirData_Stale;
    } else {
      airData.pmStatus = AirData_Ok;
    }

    nextFrame = now + 64; // 1000 / 64 = ~15 fps
    presentation.paintDisplay(now);
  }

  if (now - lastSample > sampleGap) {
    lastSample = now;

    measureDHT();

    myservo.write(map(pmsSensor.pm2_5, 0, 30, 180, 0));

    if (!tcpClient->connected() && atoi(gps_port) > 0) tcpClient->connect(WiFi.gatewayIP(), atoi(gps_port));

    long earlierLastReading = lastPmReading < lastDHTReading ? lastPmReading : lastDHTReading;
    snprintf(msg, sizeof(msg), "{\"pm2\":%u,\"pm1\":%u,\"pm10\":%u,\"l\":%s%d.%d,\"n\":%s%d.%d,\"u\":%u,\"t\":%0.1f,\"h\":%0.1f}",
             pmsSensor.pm2_5, pmsSensor.pm1, pmsSensor.pm10, lats > 0 ? "" : "-", latw, latf, lngs > 0 ? "" : "-", lngw, lngf, earlierLastReading / 60000, airData.temperature, airData.humidity);

    *errorMsg = 0;
    if (lastSample - lastPmReading > 30000) {
      snprintf(errorMsg, sizeof(errorMsg), "{\"lastSample\": %u, \"lastPmReading\": %u}", lastSample, lastPmReading);
#ifndef NO_AUTO_SWAP
      if (now - lastSwap > 60000) {
        Serial.println("swapping from here");
        Serial.flush();
        Serial.swap();
        Serial.println("swapped to here");
        lastSwap = now;
      }
#endif
    }
  }

  if (now - lastReport > reportGap && mqttConnect()) {
    lastReport = now;

    if(!client->publish(particle_topic_name, msg))
      Serial.printf("failed to send %s", msg);
    if (*errorMsg) {
      client->publish(error_topic_name, errorMsg);
      *errorMsg = '\0';
    }
  }
}
