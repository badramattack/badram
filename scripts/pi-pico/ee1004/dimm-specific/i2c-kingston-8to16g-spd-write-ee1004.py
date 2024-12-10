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


print("Warning, this is specific to the Kingston 8G RAMs!")

# Write page 0
i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x04, 0x85]), True)
i2c.writeto(0x50, bytes([0x04, 0x86]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x05, 0x21]), True)
i2c.writeto(0x50, bytes([0x05, 0x29]), True)
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7E, 0x7C]), True)
i2c.writeto(0x50, bytes([0x7E, 0x0C]), True) 
time.sleep(.2)

#i2c.writeto(0x36, bytes(0), True)
## ORIG: i2c.writeto(0x50, bytes([0x7F, 0xBF]), True)
i2c.writeto(0x50, bytes([0x7F, 0xDA]), True) 
time.sleep(.2)


power.value(0)


