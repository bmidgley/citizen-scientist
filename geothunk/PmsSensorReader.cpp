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
