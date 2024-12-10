from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDDSPD pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Issue RPSn instructions
for i, rsp in enumerate([0x31, 0x34, 0x35, 0x30]):
    try:
        # If write protection has been set for this block, the device will
        # answer with NoAck, causing an exception. Otherwise, the protection
        # has not been set for this block.
        _ = i2c.readfrom(rsp, 1)
        print(f"Block {i} is not write protected")
    except:
        print(f"Block {i} is write protected")

# Power down SPD chip
power.value(0)
