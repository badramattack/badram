from machine import Pin, I2C
import time
import struct

def dump_page(i2c: I2C, pagenb: int) -> bytes:
  # First set the page in MR11
  mr11 = pagenb >> 1
  mr11 = struct.pack("B", mr11)
  i2c.writeto_mem(0x50, 0xb, mr11)

  # Read the entire page
  addr = (1 << 7) | ((pagenb & 1) << 6)
  return i2c.readfrom_mem(0x50, addr, 64)

power = Pin(2, Pin.OUT)  # VDD_MGMT pin

# Power on SPD chip
power.value(1)
time.sleep(0.1)

# Create I2C object
i2c = I2C(0, freq=400000, scl=Pin(5), sda=Pin(4))

data = bytes()
for i in range(16):
  data += dump_page(i2c, i)

print(data.hex())

# Power down SPD chip
power.value(0)
