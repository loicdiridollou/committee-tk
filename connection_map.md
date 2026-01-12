# Connection Map

## Information

This document maintains a guide for the connections of all the elements into the
Arduino.

## Elements

### Compass

The module is the BMM150.

| Wire | Pin  |
| ---- | ---- |
| VCC  | 3.3V |
| GND  | GND  |
| SDA  | SDA  |
| SCL  | SCL  |

### GPS

The module is the BN-220.

| Wire | Pin             |
| ---- | --------------- |
| VCC  | 5V              |
| GND  | GND             |
| TX   | D0 (Arduino RX) |
| RX   | D1 (Arduino TX) |

### Anemometer

The module is the RS-FSJT-NPN.

| Wire               | Pin   |
| ------------------ | ----- |
| Brown wire (VCC)   | 5V    |
| Black wire (GND)   | GND   |
| Blue wire (Signal) | D2/D3 |

### Wind Direction

The module is the CYC-FX1 (V2).

| Wire                        | Pin           |
| --------------------------- | ------------- |
| Red wire (VCC)              | 5V            |
| Black wire (GND)            | GND           |
| Yellow wire (Signal/Output) | Analog Pin A0 |

### SD Card reader

The module includes the 74HC125 and AMS1117 chips.

| Wire        | Pin    |
| ----        | ------ |
| VCC  orange | 5V     |
| GND  black  | GND    |
| MISO grey   | Pin 12 |
| MOSI purple | Pin 11 |
| SCK  blue   | Pin 13 |
| CS   green  | Pin 10 |
