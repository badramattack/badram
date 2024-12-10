import machine
from machine import Pin
import time

# Power pin
power = Pin(2, Pin.OUT)
power.value(0)
time.sleep(.1)
power.value(1)
time.sleep(.3)

# Create I2C object
i2c = machine.I2C(0, freq=100000, scl=machine.Pin(5), sda=machine.Pin(4))

# Print out any addresses found
devices = i2c.scan()

if devices:
    for d in devices:
        print("ID: " + hex(d))
else:
    print("No devices found")

print("Warning, this is specific to the Micron 16G RAMs - setting to 32G!")

# Write page 0
i2c.writeto(0x36, bytes(0), True)
## Orig:
# i2c.writeto(0x50, bytes([0x04, 0x85]), True)
i2c.writeto(0x50, bytes([0x04, 0x86]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## Orig:
# i2c.writeto(0x50, bytes([0x05, 0x29]), True)
i2c.writeto(0x50, bytes([0x05, 0x29]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## Orig:
# i2c.writeto(0x50, bytes([0x7E, 0x2D]), True)
i2c.writeto(0x50, bytes([0x7E, 0xD2]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## Orig:
# i2c.writeto(0x50, bytes([0x7F, 0x90]), True)
i2c.writeto(0x50, bytes([0x7F, 0x55]), True)
time.sleep(.2)

##### Now change a byte in serial number...
# Power pin
power = Pin(2, Pin.OUT)
power.value(0)
time.sleep(.1)
power.value(1)
time.sleep(.3)

i2c.writeto(0x37, bytes(0), True)

## ORIG: i2c.writeto(0x50, bytes([0x45, 0x33]), True)
i2c.writeto(0x50, bytes([0x45, 0x35]), True)
time.sleep(.2)

