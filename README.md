# ESP32-S3 CANopen CO4017A0 mock

This firmware runs an ESP32-S3 as a CANopen slave with the CO4017A0 device
identity. It uses Espressif's CANopenNode component and the ESP-IDF on-chip
TWAI controller.

The Object Dictionary implements device identity, an SDO server and
asynchronous PDO communication/mapping records, plus the CO4017A0 digital and
analogue input and output objects described in `main/CO4017A0.eds`. Digital
and analogue inputs start at zero; output values retain SDO/RPDO writes in RAM.
The remaining optional EDS objects are not implemented. CANopenNode handles
standard NMT commands and produces heartbeat messages; the default heartbeat
period is 1000 ms (set to 0 to disable it). NMT state changes are logged.

The Emergency producer records the most recent eight EMCY events in object
`0x1003`. For testing, write `1` to SDO object `0x2F00:00` to send a simulated
generic-error EMCY, and write `0` to clear it. This simulator-only object is
not part of the CO4017A0 EDS.

The default node ID is 1, bitrate is 250 kbit/s, heartbeat period is 1000 ms,
and TWAI TX/RX use GPIOs 15/16. Change these under **CO4017A0 CANopen mock** in
`idf.py menuconfig`. The settings must match the CAN bus and transceiver wiring.

Build and flash with ESP-IDF 5.5.4 or newer. In PowerShell:

```text
idf.py -B build "-DSDKCONFIG=$PWD\build\sdkconfig" build
idf.py -B build "-DSDKCONFIG=$PWD\build\sdkconfig" flash monitor
```
