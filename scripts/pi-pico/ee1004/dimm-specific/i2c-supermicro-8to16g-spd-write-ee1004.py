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

print("Warning, this is specific to the Micron 8G RAMs!")

# Write page 0
i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x04, 0x85]), True)
i2c.writeto(0x50, bytes([0x04, 0x86]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x05, 0x21]), True)
i2c.writeto(0x50, bytes([0x05, 0x29]), True)
time.sleep(.2)


# i2c.writeto(0x50, bytes([0x0C, 0x01]), True)
## MOD: i2c.writeto(0x50, bytes([0x0C, 0x00]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7E, 0xFF]), True)
i2c.writeto(0x50, bytes([0x7E, 0x8F]), True) 
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7F, 0xDF]), True)
i2c.writeto(0x50, bytes([0x7F, 0xBA]), True) 
time.sleep(.2)

# Power pin
power = Pin(2, Pin.OUT)
power.value(0)
time.sleep(.1)
power.value(1)
time.sleep(.3)

i2c.writeto(0x37, bytes(0), True)

## ORIG: i2c.writeto(0x50, bytes([0x4d, 0x31]), True)
#i2c.writeto(0x50, bytes([0x4d, 0x32]), True)
#time.sleep(.2)

## ORIG: i2c.writeto(0x50, bytes([0x4F, 0x37]), True)
#i2c.writeto(0x50, bytes([0x4F, 0x38]), True)
#time.sleep(.2)

## ORIG: i2c.writeto(0x50, bytes([0x46, 0xA7]), True)
#i2c.writeto(0x50, bytes([0x46, 0xA8]), True)
#time.sleep(.2)

