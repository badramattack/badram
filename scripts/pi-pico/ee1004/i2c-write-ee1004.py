from machine import Pin, I2C
import time
import struct

from crc16 import crc16

spd = "replace with 512-byte hex string"
bspd = bytes.fromhex(spd)

# Larger block sizes resulted in incomplete writes
WRITE_SIZE = 16

power = Pin(2, Pin.OUT)  # VDDSPD pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Check the SPD protection status, we can only write if the SPD chip is unlocked
print("Checking spd protection... ", end="")
protected = False
for i, rsp in enumerate([0x31, 0x34, 0x35, 0x30]):
    try:
        # If write protection has been set for this block, the device will
        # answer with NoAck, causing an exception. Otherwise, the protection
        # has not been set for this block.
        _ = i2c.readfrom(rsp, 1)
    except:
        protected = True

if not protected:
    print("Not protected")
else:
    raise Exception(f"SPD is protected.")

# Check if the CRC is correct in the modified SPD image
print("Checking CRC... ", end="")
crc = crc16(bspd[:0x7e])
ee1004crc = bspd[0x7e:0x80]

if crc == ee1004crc:
    print("OK")
else:
    print(f"Incorrect CRC expected {crc} but got {ee1004crc}")
    print("Updating CRC... ", end="")
    bspd = bspd[:0x7e] + crc + bspd[0x80:]
    print("Done")

# Writing the SPD contents to the eeprom
print("Writing data to spd chip... ", end="")
# Set page to zero
i2c.writeto(0x36, bytes([0x00]))
for i in range(0, 256, WRITE_SIZE):
    i2c.writeto_mem(0x50, i, bspd[i:i+WRITE_SIZE])
    time.sleep(0.1)
    
i2c.writeto(0x37, bytes([0x00]))
for i in range(256, 512, WRITE_SIZE):
    i2c.writeto_mem(0x50, i-256, bspd[i:i+WRITE_SIZE])
    time.sleep(0.1)
    
print("Done")

# Power down SPD chip
power.value(0)
