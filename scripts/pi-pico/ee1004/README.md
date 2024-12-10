# Raspberry Pi Pico MicroPython scripts for dealing with SPD EEPROMs

This folder contains MicroPython scripts to read/unlock/overwrite the SPD chips on DDR4 DIMMs. 

## Hardware connections

| DDR4 DIMM pin | Description | Connection | Note |
|---------------|-------------|-------------|------|
| 284 | VDDSPD | Pico GP2  ||
| 141 | SCL    | Pico GP5  | Add pullup resistor |
| 285 | SDA    | Pico GP4  | Add pullup resistor |
| 139 | SA0    | Vss or 7-10V    | 7-10V required for unlocking |
| 140 | SA1    | Vss ||
| 238 | SA2    | Vss ||
| 283 | Vss    | Vss ||

## Scripts

Most scripts in this folder should be fairly self-explanatory in naming and what they currently do. 

The main ones are:

 * `i2c-dump-ee1004.py`: Dumps the contents of the connected EEPROM.
 * `i2c-temp-sensor-ee1004.py`: Tries to access the temperature sensor at I2C address `0x18` and read the manufacturer/device ID. Note that not all DIMMs might have this temperature sensor.
 * `i2c-read-protection-ee1004.py`: Issues Read Protection Status (RPS) instructions to the SPD chip to probe the protection status of all four blocks.
 * `i2c-unlock-ee1004.py`: Tries to unlock/remove the write protection, see above for required voltages on SA0/1/2.
 * `i2c-write-ee1004.py`: Write a given hex string to the SPD.
 
## Caveats

 * When you use I2C scanning like `devices = i2c.scan()`, be aware that this will read I2C ID `0x37` at some point, which then due to the way SPD EEPROMs work will select page 1 - do make sure to reset the device after/select the correct page if page 0 is required.
 
 * When writing changed SPD info to a DIMM that has previously been in the mainboard, be aware that some mainboards do cache the SPD info based on the serial number in the SPD. So make sure to change (e.g. increment) the serial number so that the mainboard actually takes e.g. the increased capacity.
 