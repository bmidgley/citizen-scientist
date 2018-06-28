# citizen-scientist

Monitor air quality and automatically share the collected data using inexpensive hardware.

[OpenWest slides](https://docs.google.com/presentation/d/14199zjeJYKTTuAwEFpema6mJRyAuVe0BQ5JEuAFFPP4/edit?usp=sharing) show how to build the project, including slide 24 with wiring explanation.

## Setup

Install [arduino](https://www.arduino.cc/en/Main/Software) 1.8.2 or newer

Install serial driver from [vendor drivers](https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers)

Set up [esp8266 runtime](http://esp8266.github.io/Arduino/versions/2.0.0/doc/installing.html) for arduino

Select board. Tools->Board->NodeMCU 1.0 (ESP-12E Module)

Libraries: sketch->include library->manage libraries
* WiFiManager
* ArduinoJson
* PubSubClient
* ArduinoOTA
* SimpleDHT
* "ESP8266 and ESP32 Oled Driver for SSD1306 display"
* Esp8266TrueRandom (wget https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip; sketch->include library->Add .zip Library)

Plug in board and choose the port. On mac: Tools->Port->/dev/cu.SLAB_USBtoUART (Reboot and check your USB cable is a full power+data cable if the port isn't available.)

Open geothunk.ino in arduino and hit the upload button.

Power on the board and use your phone to connect to the Geothunk-XXX access point. In the captive portal screen, set the password for your access point. Hit save. There is a [youtube video](https://www.youtube.com/watch?v=-iKyWOWEP4E&t=4s) to show this in practice.

If you're on the same access point as the sensor, you should be able to see graphs for your measurements by clicking its [geothunk.local](http://geothunk.local) mdns link. If mdns isn't working or you have multiple sensors, point a browser at the IP address displayed on the device instead.

Register and agree to share your data with our [agreement](https://docs.google.com/forms/d/e/1FAIpQLScs9sQVZbnWg0XCuS6sA2pAkzf4LLobDE_Wj_pccsHPoGVKmw/viewform)

## Contributors

* Brad Midgley wrote the firmware and built the sensor
* Dorian Tolman designed the case

Parts:
* Esp8266 [D-Duino](https://www.aliexpress.com/item/new-NODEMCU-wifi-NodeMCU-forArduino-ESP8266-wemos-for-OLED/32802190441.html)
* [DHT11](https://www.aliexpress.com/item/New-DHT11-Temperature-and-Relative-Humidity-Sensor-Module-for-arduino/1873305905.html) (3-wire package)
* [PMS3003](https://www.aliexpress.com/item/Laser-PM2-5-DUST-SENSOR-PMS3003-High-precision-laser-dust-concentration-sensor-digital-dust-particles-G3/32371229255.html)
