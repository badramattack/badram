from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDDSPD pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=400000, scl=Pin(5), sda=Pin(4))

# Select and read page 0
i2c.writeto(0x36, bytes([0]))
data = i2c.readfrom(0x50, 256)
print(data.hex(), end="")

# Select and read page 1
i2c.writeto(0x37, bytes([0]))
data = i2c.readfrom(0x50, 256)
print(data.hex())

# Power down SPD chip
power.value(0)
