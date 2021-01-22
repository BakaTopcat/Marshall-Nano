# marshall-cam-visca-control
Marshall camera RCP

I have created a tiny RCP for Marshall cameras controlled by RS485 using VISCA protocol.

Hardware used:
- Arduino Nano
- 128x64 I2C OLED
- Rotary encoder with push function
- UART to RS-485 adapter

Before uploading the RCP sketch, the **marshall-visca-write-eeprom-1** has to be uploaded. This sketch writes to EEPROM all necessary data.
