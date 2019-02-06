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
