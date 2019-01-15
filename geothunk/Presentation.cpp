#include "Presentation.h"
#include <WiFiManager.h>

int cycling(long now, int width) {
  return -(now/64 % width);
}


char *how_good(unsigned int v) {
  if (v == 0) return "Air quality seems perfect ";
  if (v < 8) return "Air quality is good ";
  if (v < 15) return "Air quality is fair ";
  if (v < 30) return "Air quality is bad ";
  if (v < 50) return "Air quality is very bad ";
  return "Air quality is dangerous ";
}

Presentation::Presentation(OLEDDisplay *display, AirData *airData) {
  this->airData = airData;
  this->display = display;

  for (gindex = POINTS - 1; gindex > 0; gindex--) graph[gindex] = graph2[gindex] = 0;
}

void Presentation::graphSet(unsigned short int *a, int points, int p0, int p1, int idx) {
  int max = 15;

  for (int i = 0; i < points; i++) {
    if (max < a[i]) max = a[i];
  }
  if (max > 0) {
    for (int i = 0; i < points; i++)
      display->drawLine(i, p0, i, p0 - ((p0-p1) * a[(i + idx) % points] / max));
  }
}

void Presentation::paintConnectingWifi(String ap_name) {
  this->paintDisplay(0);

  display->setColor(INVERSE);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(display->getWidth(), 0, WiFi.SSID());
  display->drawString(display->getWidth(), 10, "...or connect to...");
  display->drawString(display->getWidth(), 20, ap_name);
  display->display();
}

void Presentation::paintConnectingMqtt() {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(display->getWidth(), display->getHeight() / 2 - 5, String("Connecting to Server"));
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, display->getHeight() - 20, String(WiFi.SSID()));
  display->drawString(0, display->getHeight() - 10, WiFi.localIP().toString());
  display->display();
}

void Presentation::paintDisplay(long now) {
  String uptime;
  String pmStatus = (this->airData->pmStatus != AirData_Ok) ? String("ERROR ") : String(how_good(this->airData->pm2_5));
  int location;
  int width;
  long hours = now / (60 * 60 * 1000);
  long days = hours / 24;

  display->clear();
  display->setColor(WHITE);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  if (now > 0) {
    width = display->getStringWidth(pmStatus);
    location = cycling(now, width);
    display->drawString(location, 0, pmStatus);
    display->drawString(location + width, 0, pmStatus);
  }
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);

  String pmValues = String("1/2/10=");
  String humidityString;
  String temperatureString;

  if (airData->pmStatus == AirData_Stale)
    pmValues = pmValues + String("E/E/E");
  else
    pmValues = pmValues + String(airData->pm1) + String("/") + String(airData->pm2_5) + String("/") + String(airData->pm10);

  if (airData->tempHumidityStatus == AirData_Stale) {
    humidityString = String("E");
    temperatureString = String("E");
  } else {
    humidityString = String(airData->humidity);
    temperatureString = String(airData->temperature);
  }
  display->drawString(display->getWidth(), 34, pmValues);
  display->drawString(display->getWidth(), 44, humidityString + String("%h"));
  display->drawString(display->getWidth(), 54, temperatureString + String("°C"));

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  if (hours < 24)
    uptime = String(hours) + String("h");
  else if(days < 30)
    uptime = String(days) + String("d");
  else
    uptime = String(days/30) + String("m");
  display->drawString(0, 34, uptime + String(" v") + String(VERSION));
  display->drawString(0, display->getHeight() - 20, String(WiFi.SSID()));
  if (WiFi.status() == WL_CONNECTED) {
    display->drawString(0, display->getHeight() - 10, WiFi.localIP().toString());
  } else {
    display->drawString(0, display->getHeight() - 10, "No wifi connection");
  }
  if(airData->pmStatus != AirData_Uninitialized) {
    display->setColor(INVERSE);
    this->graphSet(graph, POINTS, 36, 12, gindex);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_24);
    width = display->getStringWidth(String(airData->pm2_5));
    display->setColor(BLACK);
    display->fillRect(0, 0, width + 23, 34);
    display->setColor(WHITE);
    display->drawString(0, 4, String(airData->pm2_5));
    display->setFont(ArialMT_Plain_16);
    display->drawString(width, 0, String("µg"));
    display->drawLine(width + 3, 18, width + 15, 18);
    display->drawString(width + 2, 17, String("m³"));
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 24, "2.5");
  }
  display->display();
}

void Presentation::recordGraphTemperature() {
  graph2[gindex] = airData->temperature;
}

void Presentation::recordGraphPm2() {
  graph[gindex] = airData->pm2_5;
  gindex = (gindex + 1) % POINTS;
}
