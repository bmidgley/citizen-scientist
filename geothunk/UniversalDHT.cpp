/*
  Universal auto-detecting DHT11 or DHT22 sensor.

  Copyright (c) 2019 Tim Harper

  https://github.com/timcharper

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

/**
 * This module is a rewrite of SimpleDHT: https://github.com/winlinvip/SimpleDHT
 *
 * SimpleDHT is Copyright (c) 2016-2017 winlin, and is also licensed MIT
 */

#include "UniversalDHT.h"

UniversalDHT::UniversalDHT(int pin) {
  this->pin = pin;
}

bool waitWhileValue(int pin, byte value) {
  unsigned long time_start = micros();
  long time = 0;

  while (digitalRead(pin) == value) {
    delayMicroseconds(6);
    time = micros() - time_start;
    if ((time < 0) || (time > VALUE_TIMEOUT)) {
      return false;
    }
  }
  return true;
}

UniversalDHT::Response parseDHT11(short rawTemperature, short rawHumidity, float* ptemperature, float* phumidity) {
  UniversalDHT::Response ret = {UniversalDHT::Response::success, 0};

  if (ptemperature) {
    *ptemperature = (float)(rawTemperature>>8);
  }
  if (phumidity) {
    *phumidity = (float)(rawHumidity>>8);
  }
  return ret;
}

UniversalDHT::Response parseDHT22(short rawTemperature, short rawHumidity, float* ptemperature, float* phumidity) {
  UniversalDHT::Response ret = {UniversalDHT::Response::success, 0};

  if (ptemperature) {
    *ptemperature = (float)((rawTemperature & 0x8000 ? -1 : 1) * (rawTemperature & 0x7FFF)) / 10.0;
  }
  if (phumidity) {
    *phumidity = (float)rawHumidity / 10.0;
  }

  return ret;
}

UniversalDHT::Response UniversalDHT::read(float* ptemperature, float* phumidity) {
  RawReading reading;
  Response ret = sample(&reading);
  if (ret.error) return ret;


  byte expect = reading.humidity16 + reading.humidity8 + reading.temperature16 + reading.temperature8;
  if (reading.checksum != expect) {
    ret.error = UniversalDHT::Response::errDataChecksum;
    return ret;
  }

  short rawHumidity = ((short)reading.humidity16 << 8) | reading.humidity8;;
  short rawTemperature = ((short)reading.temperature16 << 8) | reading.temperature8;;

  /**
   * DHT11 min humidity sensitivity is 20 and is reported as 20, 0
   * DHT22 max humidity is 100 and is reported as 3, 232
   * A value of 4, x or greater can be safely assumed as a DHT11 reading (it would be 102.4% humidity interpretted as DHT22)
   */
  if (reading.humidity16 > 4)
    return parseDHT11(rawHumidity, rawTemperature, ptemperature, phumidity);
  else
    return parseDHT22(rawHumidity, rawTemperature, ptemperature, phumidity);
}


inline int advance(long &last_frame, long &next_frame) {
  last_frame = next_frame;
  next_frame = micros();
  return next_frame - last_frame;
}

UniversalDHT::Response UniversalDHT::sample(RawReading *reading) {
  long lf, nf = 0;
  Response ret = {Response::success, 0};

  // https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);            // Step 1: send start signal
  delay(20);                         // DHT11: at least 18ms; DHT22: at least 1ms

  // Pull high and set to input, before wait 40us.
  digitalWrite(pin, HIGH);           // 2.
  delayMicroseconds(25);             // specs [2]: 20-40us

  advance(lf, nf);

  // pin_mode change can take variable amounts of time to return; starting our timer measurement before calling pinMode
  // is paramount for reliable timings
  pinMode(pin, INPUT);

  // DHT11 starting:
  //    1. PULL LOW 80us
  //    2. PULL HIGH 80us
  waitWhileValue(pin, LOW);
  int t = advance(lf, nf);
  if (t < 30) {                    // specs [2]: 80us
    ret.time = t;
    ret.error = Response::errStartLow;
    return ret;
  }

  waitWhileValue(pin, HIGH);             // 2.
  t = advance(lf, nf);
  if (t < 50) {                    // specs [2]: 80us
    ret.time = t;
    ret.error = Response::errStartHigh;
    return ret;
  }


  byte *ptr = (byte *)reading;
  // DHTxx data transmission:
  //    1. 1bit start, PULL LOW 50us
  //    2. PULL HIGH:
  //         - 26-28us, bit(0)
  //         - 70us, bit(1)
  for (byte byte_idx = 0; byte_idx < sizeof(RawReading); byte_idx ++) {
    ptr = (byte*)reading + byte_idx;
    *ptr = 0;
    for (signed char bit_idx = 7; bit_idx >= 0; bit_idx --) {
      waitWhileValue(pin, LOW);          // 1.
      t = advance(lf, nf);
      if (t < 24) {                    // specs says: 50us
        ret.time = t;
        ret.error = Response::errDataLow;
        return ret;
      }

      // read a bit
      waitWhileValue(pin, HIGH);              // 2.
      t = advance(lf, nf);
      if (t < 11) {                     // specs say: 26-28us
        ret.time = t;
        ret.error = Response::errDataRead;
        return ret;
      }

      *ptr = *ptr | ((t > 40 ? 1 : 0) << bit_idx);     // specs: 26-28us -> 0, 70us -> 1
    }
  }

  // DHT11 EOF:
  //    1. PULL LOW 50us.
  waitWhileValue(pin, LOW);                     // 1.
  t = advance(lf, nf);
  if (t < 24) {                           // specs say: 50us
    ret.time = t;
    ret.error = Response::errDataEOF;
  }

  return ret;
}
