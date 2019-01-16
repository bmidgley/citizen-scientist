#include "OLEDDisplay.h"
#include "geothunk.h"

#define POINTS 128

class Presentation {
private:
  unsigned short int graph[POINTS];
  unsigned short int graph2[POINTS];
  void graphSet(unsigned short int *a, int points, int p0, int p1, int idx);
  int gindex = 0;

public:
  Presentation(OLEDDisplay *display, AirData *airdata);
  void paintDisplay(long now);
  void paintConnectingWifi();
  void paintServingAp(String ap_name);
  void paintConnectingMqtt(long now);
  void recordGraphTemperature();
  void recordGraphPm2();
  OLEDDisplay *display;
  AirData *airData;
};
