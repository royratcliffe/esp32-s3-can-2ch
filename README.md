# ESP32-S3 CANopen CO4017A0 mock

This firmware runs an ESP32-S3 as a CANopen slave with the CO4017A0 device
identity. It uses Espressif's CANopenNode component and the ESP-IDF on-chip
TWAI controller.

The Object Dictionary implements device identity, an SDO server and
asynchronous PDO communication/mapping records, plus the CO4017A0 digital and
analogue input and output objects described in `main/CO4017A0.eds`. Digital
input byte 1 starts at `0xEE`; the other digital and analogue inputs start at
zero. Output values retain SDO/RPDO writes in RAM.
It also exposes the EDS SDO parameters `0x1005` (SYNC COB-ID, default `0x80`),
`0x100C` (Guard Time) and `0x100D` (Life Time Factor), initially zero for the
latter two. These are stored in RAM; changing them does not enable SYNC or
node-guarding protocol processing. The remaining optional EDS objects are not
implemented. CANopenNode handles
standard NMT commands and produces heartbeat messages; the default heartbeat
period is 1000 ms (set to 0 to disable it). NMT state changes are logged, and
NMT Reset Node restarts the ESP32.

To simulate inputs, write to `0x2F01:01` through `:02` (unsigned 8-bit digital
bytes) or `0x2F02:01` through `:08` (signed 16-bit analogue values). These
simulator-only SDOs share the values exposed read-only at `0x6000` and `0x6401`.
A changed value requests transmission of each event-driven TPDO mapping it,
subject to the NMT state and TPDO inhibit time.

For PDO diagnostics, enable **Log PDO processing events** under **CO4017A0
CANopen mock** in `idf.py menuconfig`. The log reports RPDOs pending processing
and TPDOs scheduled for transmission while operational; it does not confirm
successful CAN transmission. Logging can affect timing.

To log received CAN frames, enable **Log received CAN frames** in the same
menu. Logs include the identifier, DLC, frame type and available data bytes.
Capture uses a bounded queue; a warning reports dropped log records when it
fills. Logging can affect timing.

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
