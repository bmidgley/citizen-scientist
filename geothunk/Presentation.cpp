/*
  Presentation class for Geothunk.

  Copyright (c) 2019 Brad Midgley

  https://github.com/bmidgley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

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

void Presentation::paintConnectingWifi() {
  this->paintDisplay(0);

  display->setColor(INVERSE);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(display->getWidth(), 0, "Connecting to WIFI:");
  display->drawString(display->getWidth(), 10, WiFi.SSID());
  display->drawString(display->getWidth(), 20, "Hold Flash now to config");
  display->display();
}

void Presentation::paintServingAp(String ap_name) {
  this->paintDisplay(0);

  display->setColor(INVERSE);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(display->getWidth(), 0, "Configure this device");
  display->drawString(display->getWidth(), 10, "by connecting to");
  display->drawString(display->getWidth(), 20, ap_name);
  display->display();
}

void Presentation::paintConnectingMqtt(long now) {
  this->paintDisplay(now);
  display->setColor(INVERSE);
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
  else if (airData->pmStatus == AirData_Uninitialized)
    pmValues = pmValues + String("-/-/-");
  else
    pmValues = pmValues + String(airData->pm1) + String("/") + String(airData->pm2_5) + String("/") + String(airData->pm10);

  if (airData->tempHumidityStatus == AirData_Stale) {
    humidityString = String("E");
    temperatureString = String("E");
  } else if (airData->tempHumidityStatus == AirData_Uninitialized) {
    humidityString = String("-");
    temperatureString = String("-");
  } else {
    char format_buffer[10];
    snprintf(format_buffer, sizeof(format_buffer), "%0.0f", airData->humidity);
    humidityString = String(format_buffer);
    snprintf(format_buffer, sizeof(format_buffer), "%0.1f", airData->temperature);
    temperatureString = String(format_buffer);
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
