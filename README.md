# ESP32 UART To Syslog Gateway

This project provides a UART to syslog gateway for up to two individually configurable UART ports. The syslog output supports [RFC 5424](https://datatracker.ietf.org/doc/html/rfc5424#section-6) UDP frames as well as raw UDP packets per text line received from the serial port(s). The software's intended purpose is to capture console output of embedded systems in a continuous and comfortable way for analysis or archiving.

I currently use it to capture the console output of an AnkerMake M5C 3D printer, which logs via its serial line at 3 Mbaud. The included configuration file `sdkconfig.esp32dev-ankermake` is provided for that purpose.

### Technical Note

The allocation of CPU cores was carefully chosen in order to allow the ESP32 to keep track with burst of messages, which can
carry a significant number of bytes and lines. For the AnkerMake M5C this is especially true during booting, but with the following
CPU affinity of OS tasks, the 'FIFO full' events were eliminated:

* CORE 0: UART driver and application code for processing serial data
* CORE 1: Wifi driver and LwIP stack

### Example log from AnkerMake M5C

```
2024-01-18T22:46:52.124600+01:00 uart-syslog AnkerMakeM5C[uart1] U-Boot SPL 2013.07 (May 24 2023 - 18:22:12)
[..]
2024-01-18T22:46:52.180134+01:00 uart-syslog AnkerMakeM5C[uart1] [0.000000] xburst2 rtos @ Aug 31 2022 20:53:31, epc: 2014c496
2024-01-18T22:46:55.151687+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] Linux version 4.4.94 (pepper@ThinkStation-P720) (gcc version 7.2.0 (Ingenic Linux-Release5.1.0-Default(xburst2(fp64)+glibc2.29) 2021.12-22 10:52:10) ) #191 SMP PREEMPT Fri Dec 22 09:56:
2024-01-18T22:46:55.151687+01:00 uart-syslog AnkerMakeM5C[uart1] 17 CST 2023
2024-01-18T22:46:55.152276+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] CPU0 RESET ERROR PC:FFFFFFFF
2024-01-18T22:46:55.152752+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] bootconsole [early0] enabled
2024-01-18T22:46:55.153378+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] CPU0 revision is: 00132000 (Ingenic XBurst@II.V2)
2024-01-18T22:46:55.154511+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] FPU revision is: 00f32000
2024-01-18T22:46:55.154511+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] MSA revision is: 00002000
2024-01-18T22:46:55.155178+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] MIPS: machine is ingenic,x2000_module_base
[..]
2024-01-18T22:46:55.167571+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] Dentry cache hash table entries: 16384 (order: 4, 65536 bytes)
2024-01-18T22:46:55.169394+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] Inode-cache hash table entries: 8192 (order: 3, 32768 bytes)
2024-01-18T22:46:55.169394+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.000000] Memory: 112840K/122880K available (5820K kernel code, 310K rwdata, 1848K rodata, 268K init, 365K bss, 10040K reserved, 0K cma-
reserved, 0K highmem)
[..]
2024-01-18T22:46:56.101942+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.588963] mmcblk0: mmc0:0001 8GTF4R 7.28 GiB 
2024-01-18T22:46:56.113319+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.599412] mmcblk0boot0: mmc0:0001 8GTF4R partition 1 4.00 MiB
2024-01-18T22:46:56.113319+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.599940] mmcblk0boot1: mmc0:0001 8GTF4R partition 2 4.00 MiB
2024-01-18T22:46:56.113319+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.600492] mmcblk0rpmb: mmc0:0001 8GTF4R partition 3 512 KiB
2024-01-18T22:46:56.113708+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.602010] Alternate GPT is invalid, using primary GPT.
2024-01-18T22:46:56.114391+01:00 uart-syslog AnkerMakeM5C[uart1] [    0.602396]  mmcblk0: p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11
[..]
2024-01-18T22:46:57.139638+01:00 uart-syslog AnkerMakeM5C[uart1] [    1.626928] Dongle Host Driver, version 1.363.125.7 (r)
2024-01-18T22:46:57.139638+01:00 uart-syslog AnkerMakeM5C[uart1] [    1.626928] Compiled in /home/pepper/release_test/V8110_x1000/V8310_x1000/module_driver/devices/wireless/bcmdhd_1.363.125.7 on Sep 25 2023 at 14:40:24
2024-01-18T22:46:57.140401+01:00 uart-syslog AnkerMakeM5C[uart1] [    1.628686] Register interface [wlan0]  MAC: 00:90:XX:XX:XX:XX
[..]
```
After successfully establishing the Wifi connection, the following hosts are ping'ed (5 packets in a row) continuously every 20 seconds(!):
* www.microsoft.com
* www.google.com
* www.apple.com
* www.anker.com

When sending a gcode file to the printer, there are also some messages displayed for decoding the base64-encoded image embedded as comment in the gcode.
Other commands sent to the printer do not seem to generate console output.
