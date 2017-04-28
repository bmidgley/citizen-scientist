#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

bool shouldSaveConfig = false;
#define TRIGGER_PIN 0

void configure(WiFiManager &wifiManager) {
  if (!wifiManager.startConfigPortal("GeothunkAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n Starting");

  pinMode(TRIGGER_PIN, INPUT);
}


void loop() {
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    WiFiManager wifiManager;
    configure(wifiManager);
    Serial.println("connected...yeey :)");
  }
}
