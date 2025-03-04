from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))
time.sleep(0.1)

mr48 = i2c.readfrom_mem(0x50, 0x30, 1)
if mr48[0] >> 2 & 1:
    print("Write protection overwrite enabled.")
else:
    print("Write protection overwrite disabled. Check if HSA is tied to GND.")
    print("Try manually resetting the SPD by reconnecting VIN while leaving HSA conencted to GND.")
    raise Exception("Write protection overwrite not enabled.")

# Set MR12 and MR13 to zero, disabling the write protection
i2c.writeto_mem(0x50, 0xc, b"\x00")
time.sleep(0.1)
i2c.writeto_mem(0x50, 0xd, b"\x00")

# Read out MR12 and MR13
mr12 = i2c.readfrom_mem(0x50, 0xc, 1)
mr13 = i2c.readfrom_mem(0x50, 0xd, 1)

print(f"MR12={mr12[0]:#x}")
print(f"MR13={mr13[0]:#x}")

# Power down SPD chip
power.value(0)
