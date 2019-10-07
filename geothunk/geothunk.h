#define VERSION "1.25"

enum AirDataStatus {AirData_Uninitialized = 0, AirData_Ok = 1, AirData_Stale = 2};

struct AirData {
  unsigned int pm1;
  unsigned int pm2_5;
  unsigned int pm10;
  enum AirDataStatus pmStatus;

  float temperature;
  float humidity;

  enum AirDataStatus tempHumidityStatus;
};
