from machine import Pin, I2C
import time
import struct

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=400000, scl=Pin(5), sda=Pin(4))

# Read out every register
data = bytes()
for i in range(128):
  data += i2c.readfrom_mem(0x50, i, 1)

print(data.hex())

# Power down SPD chip
power.value(0)
