from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Set MR12 and MR13 to zero, disabling the write protection
i2c.writeto_mem(0x50, 0xc, b"\x00")
i2c.writeto_mem(0x50, 0xd, b"\x00")

# Power down SPD chip
power.value(0)
