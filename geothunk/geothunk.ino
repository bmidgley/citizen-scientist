//#define SPI_DISPLAY
//#define DEBUG

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
#include "SSD1306.h"
#include "ESP8266TrueRandom.h"
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SimpleDHT.h>

int pinDHT11 = D3;
SimpleDHT11 dht11;

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

#define MDNS_NAME "geothunk"
#define TRIGGER_PIN 0
#define VERSION "1.13"
#define POINTS 128

bool shouldSaveConfig = false;
long lastSample = 0;
long lastReport = 0;
long lastReading = 0;
long lastReconfigure = 0;
long lastSwap = 0;
char msg[200] = "";
char errorMsg[200] = "";
int disconnects = 0;

char mqtt_server[40] = "mqtt.geothunk.com";
char mqtt_port[6] = "8080";
char uuid[64] = "";
char gps_port[10] = "";
char ota_password[10] = "";

char particle_topic_name[128];
char error_topic_name[128];
char ap_name[64];
char *version = VERSION;

unsigned int pm1 = 0;
unsigned int pm2_5 = 0;
unsigned int pm10 = 0;
byte temperature = 0;
byte humidity = 0;

int sampleGap = 2 * 1000;
int reportGap = 60 * 1000;
int reconfigureGap = 5 * 1000;
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
unsigned short int graph[POINTS];
unsigned short int graph2[POINTS];
int gindex = 0;

WiFiClientSecure *tcpClient;
PubSubClient *client;
ESP8266WebServer *webServer;
#ifdef SPI_DISPLAY
SSD1306Spi display(D8, D1, D2); // rst n/c, dc D1, cs D2, clk D5, mosi D7
#else
SSD1306 display(0x3c, 5, 4);
#endif

const char* serverIndex = "<html><head><script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/smoothie/1.34.0/smoothie.js'></script> <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.js'></script> <script type='text/Javascript'> \
var sc = new SmoothieChart({ responsive: true, millisPerPixel: 1000, labels: { fontSize: 30, precision: 0 }, grid: { fillStyle: '#6699ff', millisPerLine: 100000, verticalSections: 10 }, yRangeFunction: function(r) { return { min: 0, max: Math.max(30, 10*Math.ceil(0.09 * r.max)) } } }); \
var line1 = new TimeSeries(); var line2 = new TimeSeries(); var line3 = new TimeSeries(); \
sc.addTimeSeries(line1, { strokeStyle:'rgb(180, 50, 0)', fillStyle:'rgba(180, 50, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line2, { strokeStyle:'rgb(255, 0, 0)', fillStyle:'rgba(255, 0, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line3, { strokeStyle:'rgb(255, 50, 0)', fillStyle:'rgba(255, 50, 0, 0.4)', lineWidth:3 }); \
$(document).ready(function() { sc.streamTo(document.getElementById('graphcanvas1')); }); \
setInterval(function() { $.getJSON('/stats',function(data){ line1.append(Date.now(), data.pm2); line2.append(Date.now(), data.pm1); line3.append(Date.now(), data.pm10); }); }, 2000); \
</script></head><body>  <canvas id='graphcanvas1' style='width:100%; height:75%;' /></body></html>";

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

void check_for_reconfigure() {
  static int reconfigure_counter = 0;

  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    if (reconfigure_counter > 0) {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.setFont(ArialMT_Plain_10);
      display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, String("Hold to clear settings"));
      display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, String(3 - reconfigure_counter));
      display.display();
    }

    reconfigure_counter++;
    if (reconfigure_counter > 2) {
      WiFi.disconnect(true);

      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.setFont(ArialMT_Plain_10);
      display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, String("Release and tap reset"));
      display.display();
    }
    return;
  } else {
    reconfigure_counter = 0;
  }
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

int handle_gps_byte(int byteGPS) {
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

char *how_good(unsigned int v) {
  if (v == 0) return "Air quality seems perfect ";
  if (v < 8) return "Air quality is good ";
  if (v < 15) return "Air quality is fair ";
  if (v < 30) return "Air quality is bad ";
  if (v < 50) return "Air quality is very bad ";
  return "Air quality is dangerous ";
}

void graph_set(unsigned short int *a, int points, int p0, int p1, int idx) {
  int max = 0;

  for (int i = 0; i < points; i++) {
    if (max < a[i]) max = a[i];
  }
  if (max > 0) {
    for (int i = 0; i < points; i++)
      display.drawLine(i, p0, i, p0 - ((p0-p1) * a[(i + idx) % points] / max));
  }
}

int cycling(long now, int width) {
  return -(now/64 % width);
}

void paint_display(long now, byte temperature, byte humidity) {
  float f = 32 + temperature * 9.0 / 5.0;
  String uptime;
  String status = String(how_good(pm2_5));
  int location;
  int width;

  display.clear();
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  if (now > 0) {
    width = display.getStringWidth(status);
    location = cycling(now, width);
    display.drawString(location, 0, status);
    display.drawString(location + width, 0, status);
  }
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(DISPLAY_WIDTH, 34, String("1/2/10=") + String(pm1) + String("/") + String(pm2_5) + String("/") + String(pm10));
  display.drawString(DISPLAY_WIDTH, 44, String(humidity) + String("%h"));
  display.drawString(DISPLAY_WIDTH, 54, String(round(f)) + String("°"));
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (now < 24 * 60 * 60 * 1000)
    uptime = String(now / (60 * 60 * 1000)) + String("h");
  else
    uptime = String(now / (24 * 60 * 60 * 1000)) + String("d");
  display.drawString(0, 34, uptime + String(" v") + String(version));
  display.drawString(0, DISPLAY_HEIGHT - 20, String(WiFi.SSID()));
  if (WiFi.status() == WL_CONNECTED) {
    display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  } else {
    display.drawString(0, DISPLAY_HEIGHT - 10, "No wifi connection");
  }
  display.setColor(INVERSE);
  graph_set(graph, POINTS, 36, 12, gindex);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  width = display.getStringWidth(String(pm2_5));
  display.setColor(BLACK);
  display.fillRect(0, 0, width + 23, 33);
  display.setColor(WHITE);
  display.drawString(0, 4, String(pm2_5));
  display.setFont(ArialMT_Plain_16);
  display.drawString(width, 0, String("µg"));
  display.drawLine(width + 3, 18, width + 15, 18);
  display.drawString(width + 2, 17, String("m³"));
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 24, "2.5");
  display.display();
}

void handleGPS() {
  if (tcpClient->connected() && tcpClient->available()) handle_gps_byte(tcpClient->read());
}

void measure() {
  int index = 0;
  int err = SimpleDHTErrSuccess;
  char value;
  char previousValue;

  err = dht11.read(pinDHT11, &temperature, &humidity, NULL);
#ifdef DEBUG
  if(err != SimpleDHTErrSuccess) {
    Serial.printf("Read DHT11 failed, err=%d", err);
  }
#endif

  graph2[gindex] = temperature;

  while (Serial.available()) {
    value = Serial.read();
    if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4d)) {
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
      graph[gindex] = pm2_5;
      gindex = (gindex + 1) % POINTS;
    } else if (index > 15) {
      break;
    }
    index++;
  }
  while (Serial.available()) {
    Serial.read();
  }
}

void setup() {
  WiFiManager wifiManager;
  bool create_ota_password = true;
  byte uuidNumber[16];
  byte uuidCode[16];

  Serial.begin(9600);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT);

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

          if (json["mqtt_server"]) strcpy(mqtt_server, json["mqtt_server"]);
          if (json["mqtt_port"]) strcpy(mqtt_port, json["mqtt_port"]);
          if (json["uuid"]) strcpy(uuid, json["uuid"]);
          if (json["gps_port"]) strcpy(gps_port, json["gps_port"]);
          if (json["ota_password"]) {
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
  if (create_ota_password) {
    Serial.println("generating ota_password");
    wifiManager.addParameter(&custom_ota_password);
    saveConfigCallback();
  }

  snprintf(ap_name, 64, "Geothunk-%d", ESP8266TrueRandom.random(100, 1000));
  Serial.printf("autoconnect with AP name %s\n", ap_name);

  for (gindex = POINTS - 1; gindex > 0; gindex--) graph[gindex] = graph2[gindex] = 0;

#ifndef NO_AUTO_SWAP
  Serial.swap();
#endif

  wifiManager.setTimeout(120);
  do {
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      WiFi.disconnect(true);
    }

    measure();
    paint_display(0, temperature, humidity);

    display.setColor(INVERSE);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(DISPLAY_WIDTH, 0, WiFi.SSID());
    display.drawString(DISPLAY_WIDTH, 10, "...or connect to...");
    display.drawString(DISPLAY_WIDTH, 20, String(ap_name));
    display.display();
  } while (!wifiManager.autoConnect(ap_name));

  Serial.println("stored wifi connected");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(gps_port, custom_gps_port.getValue());
  if (uuid == NULL || *uuid == 0) {
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
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 5, String("Connecting to Server"));
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, DISPLAY_HEIGHT - 20, String(WiFi.SSID()));
  display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  display.display();

  client = new PubSubClient(*(new WiFiClient()));
  client->setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client->setCallback(mqttCallback);

  for (int i = 0; i < 300; i++) {
    linea[i] = ' ';
  }
  tcpClient = new WiFiClientSecure();
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
  webServer->on("/", HTTP_GET, []() {
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    webServer->send(200, "text/html", serverIndex);
  });
  webServer->on("/stats", HTTP_GET, []() {
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    webServer->send(200, "application/json", msg);
  });
  webServer->begin();

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  handleGPS();
  ArduinoOTA.handle();
  webServer->handleClient();
  client->loop();

  long now = millis();
  paint_display(now, temperature, humidity);

  if(now - lastReconfigure > reconfigureGap) {
    lastReconfigure = now;

    check_for_reconfigure();
  }

  if (now - lastSample > sampleGap) {
    lastSample = now;

    measure();

    if (!tcpClient->connected() && atoi(gps_port) > 0) tcpClient->connect(WiFi.gatewayIP(), atoi(gps_port));

    snprintf(msg, 200, "{\"pm2\":%u,\"pm1\":%u,\"pm10\":%u,\"l\":%s%d.%d,\"n\":%s%d.%d,\"u\":%u,\"t\":%d,\"h\":%d}",
             pm2_5, pm1, pm10, lats > 0 ? "" : "-", latw, latf, lngs > 0 ? "" : "-", lngw, lngf, lastReading / 60000, temperature, humidity);

    *errorMsg = 0;
    if (lastSample - lastReading > 30000) {
      snprintf(errorMsg, 200, "{\"lastSample\": %u, \"lastReading\": %u}", lastSample, lastReading);
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
