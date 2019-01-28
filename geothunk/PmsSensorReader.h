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

/*
 * Controller can just feed data one byte at a time from the serial input device. This reader class will detect the
 * frame beginning.  Dropped bytes may lead to an errant reading; however, the reader will recalibrate to the next frame
 * and recover if it is continually fed data.
 */
class PmsSensorReader {
private:
  void _measure();
public:
  PmsSensorReader();
  unsigned int pm1 = 0;
  unsigned int pm2_5 = 0;
  unsigned int pm10 = 0;
  unsigned char buffer[24];
  unsigned char packetIndex = 0;

  /**
   * Offer a byte read from the reader.
   *
   * Returns true if the byte completes a PMS Sensor packet and a new reading is available
   */
  bool offer(unsigned char value);
};
