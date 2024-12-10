from machine import Pin, I2C
import time

print("Warning, this script requires 7-10V on SA0 and Vss on SA1/2!")

power = Pin(2, Pin.OUT)  # VDDSPD pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Issue Clear All Write Protection (CWP) instruction which clears the write
# protection for all blocks
i2c.writeto(0x33, bytes([0, 0]), True)

time.sleep(1)
power.value(0)
