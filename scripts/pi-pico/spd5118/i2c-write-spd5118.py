from machine import Pin, I2C
import time
import struct

from spd5118 import spd5118read, spd5118write
from crc16 import crc16

spd = "replace with hex string"
bspd = bytes.fromhex(spd)

# Larger block sizes resulted in incomplete writes
WRITE_SIZE = 16

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=100000, scl=Pin(5), sda=Pin(4))

# Check the SPD protection status, we can only write if the SPD chip is unlocked
print("Checking spd protection... ", end="")
mr12 = int.from_bytes(i2c.readfrom_mem(0x50, 0xc, 1), "little")
mr13 = int.from_bytes(i2c.readfrom_mem(0x50, 0xd, 1), "little")

if mr12 == 0 and mr13 == 0:
    print("Not protected")
else:
    print(f"SPD is protected: mr12={mr12:#04x}, mr13={mr13:#04x}")
    print("Unlocking SPD... ", end="")
    # Set MR12 and MR13 to zero, disabling the write protection
    i2c.writeto_mem(0x50, 0xc, b"\x00")  # MR12
    i2c.writeto_mem(0x50, 0xd, b"\x00")  # MR13
    print("Done")

# Writing the SPD contents to the eeprom
print("Writing data to spd chip... ", end="")

for i in range(0, len(bspd), WRITE_SIZE):
    spd5118write(i2c, i, bspd[i:i+WRITE_SIZE])
    time.sleep(0.1)
    
print("Done")

# Check if the CRC is still correct after writing the new SPD data
# If not valid, overwrite with the correct CRC
print("Checking CRC... ", end="")
crc = crc16(bspd[:0x1fe])
spd5118crc = spd5118read(i2c, 0x1fe, 2)

if crc == spd5118crc:
    print("OK")
else:
    print(f"Incorrect CRC expected {crc} but got {spd5118crc}")
    print("Updating CRC... ", end="")
    spd5118write(i2c, 0x1fe, crc)
    print("Done")

# Power down SPD chip
power.value(0)
