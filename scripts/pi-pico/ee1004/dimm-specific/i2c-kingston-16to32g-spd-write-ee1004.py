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


print("Warning, this is specific to the Kingston 16G RAMs!")

# Write page 0
i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x04, 0x86]), True)
i2c.writeto(0x50, bytes([0x04, 0x87]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x05, 0x29]), True)
i2c.writeto(0x50, bytes([0x05, 0x31]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7E, 0x38]), True)
i2c.writeto(0x50, bytes([0x7E, 0xDD]), True) 
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7F, 0x0F]), True)
i2c.writeto(0x50, bytes([0x7F, 0xBD]), True) 
time.sleep(.2)


power.value(0)


