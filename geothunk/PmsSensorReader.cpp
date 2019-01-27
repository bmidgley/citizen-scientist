/*
  Reader class for PMS3003 sensor.
  
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

#include "PmsSensorReader.h"

PmsSensorReader::PmsSensorReader() {
  this->packetIndex = 0;
  this->pm1 = 0;
  this->pm2_5 = 0;
  this->pm10 = 0;
}

bool PmsSensorReader::offer(unsigned char value) {
  this->buffer[this->packetIndex] = value;
  if (this->packetIndex == 0) {
    if(value == 0x42) {
      this->packetIndex = 1;
    }
    return false;
  }

  if (this->packetIndex == 1) {
    if (value == 0x4d) {
      this->packetIndex = 2;
      return false;
    } else {
      this->packetIndex = 0;
      return false;
    }
  }

  if (this->packetIndex == 23) {
    _measure();

    this->packetIndex = 0;
    return true;
  } else {
    this->packetIndex ++;
    return false;
  }
}


void PmsSensorReader::_measure() {
  this->pm1 = 256 * this->buffer[4] + this->buffer[5];
  this->pm2_5 = 256 * this->buffer[6] + this->buffer[7];
  this->pm10 = 256 * this->buffer[8] + this->buffer[9];
}
