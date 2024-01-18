# ESP32 UART To Syslog Gateway

This project provides a UART to syslog gateway for up to two individually configurable UART ports. The syslog output supports [RFC 5424](https://datatracker.ietf.org/doc/html/rfc5424#section-6) UDP frames as well as raw UDP packets per text line received from the serial port(s). The software's intended purpose is to capture console output of embedded systems in a continuous and comfortable way for analysis or archiving.

I currently use it to capture the console output of an AnkerMake M5C 3D printer, which logs via its serial line at 3 Mbaud. The included configuration file `sdkconfig.esp32dev-ankermake` is provided for that purpose.
