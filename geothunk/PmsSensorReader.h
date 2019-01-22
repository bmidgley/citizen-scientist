/*
  Reader class for PMS3003 sensor.
  Tim Harper
  https://github.com/timcharper
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
