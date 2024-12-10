from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Read out MR12 and MR13
mr12 = i2c.readfrom_mem(0x50, 0xc)
mr13 = i2c.readfrom_mem(0x50, 0xd)

print(f"MR12={mr12:#x}")
print(f"MR13={mr13:#x}")

# Power down SPD chip
power.value(0)
