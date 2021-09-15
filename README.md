# ESP32-based upgrade for old analog wifi amp

Adds BT streaming and auto off when no signal.
OTA firmware updating.
For more info see here.

## Control button functions

Single press: bring out of standby
Long press (>1 second): disconnect from BT client (to allow another)

Holding on power up: put into flash-mode. Set `DEVICE_NAME`  and wifi SSID & p/w in `config demo.h`. You need to put the correct mDNS device name in `platformio.ini` OTA environment too.
