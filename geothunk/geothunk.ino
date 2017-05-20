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
char msg[200];

char mqtt_server[40] = "flamebot.com";
char mqtt_port[6] = "8080";
char blynk_token[34] = "3e82307c0ad9495d900b4e5454a3e957";
char uuid[64] = "";
char gps_port[10] = "11000";
uint8_t mac[6];

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

WiFiClient tcpClient;
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
 if (client.connect(uuid)) {
  Serial.println("connected");
  //client.subscribe("lightening");
 } else {
  Serial.print("failed, rc=");
  Serial.print(client.state());
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
  
  Serial.begin(9600);
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

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);
          strcpy(uuid, json["uuid"]);
          strcpy(gps_port, json["gps_port"]);

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
  WiFiManagerParameter custom_gps_port("gps_port", "GPS port (optional)", gps_port, 10);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_gps_port);
  
  wifiManager.autoConnect("GeothunkAP");
  Serial.println("stored wifi connected");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(gps_port, custom_gps_port.getValue());
  if(*uuid == 0)
    snprintf(uuid, 64, "%02x%02x%02x%02x%02x%02x-%02x", (int)mac[5], (int)mac[4], (int)mac[3], (int)mac[2], (int)mac[1], (int)mac[0], random(255));
  
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;
    json["uuid"] = uuid;
    json["gps_port"] = gps_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");
    json.printTo(configFile);
    configFile.close();
  }

  client.setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client.setCallback(mqttCallback);

  for (int i=0;i<300;i++) {
    linea[i]=' ';
  }
  tcpClient.connect(WiFi.gatewayIP(), atoi(gps_port));
}

float to_degrees(char *begin, char *end, int &whole, int &decimal) {
  char copied[20];
  float result;
  strncpy(copied, begin, 10);
  copied[end-begin] = '\0';
  float nmea = atof(copied);
  int degrees = (int)(nmea / 100);
  nmea -= 100 * degrees;
  whole = degrees;
  decimal = (int)(nmea * ( 10000.0 / 60.0));
  result = degrees + nmea / 60.0;
  Serial.print(result, 4);
  return result;
}

int handle_gps_byte(int byteGPS) {
  // note: there is a potential buffer overflow here!
  linea[conta]=byteGPS;        // If there is serial port data, it is put in the buffer
  conta++;
  //Serial.write(byteGPS);
  if (byteGPS==13){            // If the received byte is = to 13, end of transmission
   // note: the actual end of transmission is <CR><LF> (i.e. 0x13 0x10)
   cont=0;
   bien=0;
   // The following for loop starts at 1, because this code is clowny and the first byte is the <LF> (0x10) from the previous transmission.
   for (int i=1;i<7;i++){     // Verifies if the received command starts with $GPR
     if (linea[i]==comandoGPR[i-1]){
       bien++;
     }
   }
   if(bien==6){               // If yes, continue and process the data
     for (int i=0;i<300;i++){
       if (linea[i]==','){    // check for the position of the  "," separator
         // note: again, there is a potential buffer overflow here!
         indices[cont]=i;
         cont++;
       }
       if (linea[i]=='*'){    // ... and the "*"
         indices[12]=i;
         cont++;
       }
     }
     Serial.println("");      // ... and write to the serial port
     Serial.println("");
     Serial.println("---------------");
     for (int i=0;i<12;i++){
       switch(i){
         case 0 :Serial.print("Time in UTC (HhMmSs): ");break;
         case 1 :Serial.print("Status (A=OK,V=KO): ");break;
         case 2 :Serial.print("Latitude: "); to_degrees(linea + 1 + indices[i], linea + indices[i + 1], latw, latf); break;
         case 3 :Serial.print("Direction (N/S): "); lats = linea[indices[i]+1] == 'N' ? 1 : -1; break;
         case 4 :Serial.print("Longitude: "); to_degrees(linea + 1 + indices[i], linea + indices[i + 1], lngw, lngf); break;
         case 5 :Serial.print("Direction (E/W): "); lngs = linea[indices[i]+1] == 'E' ? 1 : -1; break;
         case 6 :Serial.print("Velocity in knots: ");break;
         case 7 :Serial.print("Heading in degrees: ");break;
         case 8 :Serial.print("Date UTC (DdMmAa): ");break;
         case 9 :Serial.print("Magnetic degrees: ");break;
         case 10 :Serial.print("(E/W): ");break;
         case 11 :Serial.print("Mode: ");break;
         case 12 :Serial.print("Checksum: ");break;
       }
       for (int j=indices[i];j<(indices[i+1]-1);j++){
         Serial.print(linea[j+1]);
       }
       Serial.println("");
     }
     Serial.println("---------------");
   }
   conta=0;                    // Reset the buffer
   for (int i=0;i<300;i++){    //
     linea[i]=' ';
   }
  }
}

void loop() {
  int index = 0;
  char value;
  char previousValue;

  if(tcpClient.available()) handle_gps_byte(tcpClient.read());

  long now = millis();
  if (now - lastMsg < 2000) {
    return;
  }
  lastMsg = now;
  
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    Serial.println("disconnecting from wifi to reconfigure");
    //WiFi.disconnect(true);
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
      snprintf(msg, 200, "{\"pm1\":%u, \"pm2_5\":%u, \"pm10\":%u, \"lat\": %s%d.%d, \"lng\": %s%d.%d}",
               pm1, pm2_5, pm10, lats > 0 ? "":"-", latw, latf, lngs > 0 ? "":"-", lngw, lngf);
    } else if (index > 15) {
      break;
    }
    index++;
  }
  while(Serial.available()) {
    Serial.read();
  }
  
  if(msg[0]) {
    if (!client.connected()) {
      mqttReconnect();
    }
    //client.loop();
    client.publish("lightTopic", msg);
  }
  msg[0] = 0;
}
