/*
  Universal auto-detecting DHT11 or DHT22 sensor.

  Copyright (c) 2019 Tim Harper

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

#ifndef __UniversalDHT_H
#define __UniversalDHT_H

#include <Arduino.h>

#define VALUE_TIMEOUT 10000 // 10ms

enum UniversalDHTResult: unsigned char
{
  success = 0, // Success.
    errStartLow = 1, // Low signal came faster than expected
    errStartHigh = 2, //
    errDataLow = 3, //
    errDataRead = 4, //
    errDataEOF = 5, //
    errDataChecksum = 6 // Checksum for read data didn't match
    };



struct DHTResponse {
  UniversalDHTResult error;
  unsigned char time;
};

/**
 * Struct which encodes the protocol for the data sent by the DHT sensor
 */
struct RawReading {
  byte humidity16;
  byte humidity8;
  byte temperature16;
  byte temperature8;
  byte checksum;
};


class UniversalDHT {
 private:
  int pin;
  DHTResponse sample(RawReading *reading);
 public:
  UniversalDHT(int pin);
  DHTResponse read(float* temperature, float* humidity);
};

#endif
