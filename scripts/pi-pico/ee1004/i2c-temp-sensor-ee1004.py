from machine import Pin, I2C
import time

power = Pin(2, Pin.OUT)  # VDDSPD pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Print out any addresses found
devices = i2c.scan()

if devices:
    for d in devices:
        print("ID: " + hex(d))
else:
    print("No devices found")

# Select device ID
i2c.writeto(0x18, bytes([0x06]), False)
data = i2c.readfrom(0x18, 2)
print("Manufacturer ID: " + data.hex())

# Select product ID
i2c.writeto(0x18, bytes([0x07]), False)
data = i2c.readfrom(0x18, 2)
print("Product ID: " + data.hex())

power.value(0)
