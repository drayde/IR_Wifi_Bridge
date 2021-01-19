# IR_Wifi_Bridge

Small EPS32 based webserver that sends IR commands to a TV via an IR LED attached to the EPS32.

Update Jan-2021: Works with an off-the-shelf M5Stick-C using its built-in IR LED.

It can be used in two different ways: with a browser or with the Android app irplus.

## Browser mode

After connecting your ESP32 to your Wifi, browse to http://<eps32_ip>/ui
It will display a simple UI with buttons to control a TV.
It is hardcoded with codes for a LG TV. To use with your specific TV, change indexHtml.h. Check out https://irplus-remote.github.io/ to get code for many devices.

## irplus mode

irplus LAN is an Android app that has buildin remote layouts and codes for many, many TVs, DVD/Blu-ray players etc. irplus can be configured to send the codes for the pressed buttons to the IR_Wifi_Bridge ESP32, which then controls the attached LED accordingly.

More info about irplus LAN check out https://irplus-remote.github.io/

irplus LAN on Google Play: https://play.google.com/store/apps/details?id=net.binarymode.android.irpluslan


IR_Wifi_Bridge is a project by Simon and Andreas Kahler.

