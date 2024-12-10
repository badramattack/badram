# Raspberry Pi Pico MicroPython scripts for dealing with SPD EEPROMs

This folder contains MicroPython scripts to read/unlock/overwrite the SPD chips on DDR5 DIMMs. 

## Hardware connections

| DDR5 RDIMM pin | Description | Connection | Note |
|----------------|-------------|-------------|------|
| 3   | VIN_MGMT | 3V3  ||
| 4   | HSCL     | Pico GP5  | Add pullup resistor |
| 5   | HSDA     | Pico GP4  | Add pullup resistor |
| 148 | HSA      | Vss | I2C address 0x50 |
| 6   | Vss      | Vss       ||

| DDR5 UDIMM pin | Description | Connection | Note |
|----------------|-------------|-------------|------|
| 1   | VIN_BULK | 5V  ||
| 151 | PWR_EN   | Pico GP2  ||
| 4   | HSCL     | Pico GP5  | Add pullup resistor |
| 5   | HSDA     | Pico GP4  | Add pullup resistor |
| 148 | HSA      | Vss | I2C address 0x50 |
| 6   | Vss      | Vss       ||

The SPD5 device can support IO levels from 1.0 V to 3.3 V.

## Scripts

Most scripts in this folder should be fairly self-explanatory in naming and what they currently do. 

The main ones are:

 * `i2c-dump-spd-spd5118.py`: Dumps the contents of the connected EEPROM.
 * `i2c-dump-registers-spd5118.py`: Dumps the register contents of the connected EEPROM.
 * `i2c-read-protection-spd5118.py`: Prints the contents of registers MR12 and MR13.
 * `i2c-unlock-spd5118.py`: Remove the write protection.
 * `i2c-write-spd5118.py`: Write a given hex string to the SPD.
 
## Caveats
 * When writing changed SPD info to a DIMM that has previously been in the mainboard, be aware that some mainboards do cache the SPD info based on the serial number in the SPD. So make sure to change (e.g. increment) the serial number so that the mainboard actually takes e.g. the increased capacity.
 