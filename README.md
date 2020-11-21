# ESP32-Web-Radio-Simple

This is a fork of https://github.com/educ8s/ESP32-Web-Radio-Simple   
The project description is available on web page of original author https://educ8s.tv/esp32-internet-radio/ and this code works pretty much as the original.

### Code changes/improvements:
* Removed Nextion Display code since using this display was overkill for a simple project like this.
* Added code to support small i2C 128x32 oled screen (https://ebay.to/35OYwC2).
* Added diagnostic startup serial messages. Mostly useful during development time, but still handy when something is going wrong.
* Changed COM port speed to 115200 bits per second which is native to ESP devices.
* Changed EEPROM save and read functions since original functions gave me errors during developmend.
* Added a simple EEPROM verification routine to check the saved value.
* Added station names to be displayed on the oled screen, on serial messages and on UDP reports.
* Added UDP Server to receive staion selection command.
* Added UDP broadcast function to send keep alive messages on LAN and repor radio station name.

