# citizen-scientist

Monitor air quality and automatically share the collected data using inexpensive hardware.

[OpenWest slides](https://docs.google.com/presentation/d/14199zjeJYKTTuAwEFpema6mJRyAuVe0BQ5JEuAFFPP4/edit?usp=sharing) show how to build the project, including slide 24 with wiring explanation.

## Setup

Libraries: sketch->include library->manage libraries
* WiFiManager
* ArduinoJson
* PubSubClient
* ArduinoOTA
* SimpleDHT
* "ESP8266 and ESP32 Oled Driver for SSD1306 display"
* Esp8266TrueRandom (wget https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip; sketch->include library->Add .zip Library)

## Contributors

* Brad Midgley wrote the firmware and built the sensor
* Dorian Tolman designed the case

Parts:
* Esp8266 [D-Duino](https://www.aliexpress.com/item/new-NODEMCU-wifi-NodeMCU-forArduino-ESP8266-wemos-for-OLED/32802190441.html)
* [DHT11](https://www.aliexpress.com/item/New-DHT11-Temperature-and-Relative-Humidity-Sensor-Module-for-arduino/1873305905.html) (3-wire package)
* [PMS3003](https://www.aliexpress.com/item/Laser-PM2-5-DUST-SENSOR-PMS3003-High-precision-laser-dust-concentration-sensor-digital-dust-particles-G3/32371229255.html)
