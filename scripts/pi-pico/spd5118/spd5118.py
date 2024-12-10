from machine import I2C
import struct

BLOCK_SIZE = 64
I2C_ADDR = 0x50

def spd5118setpage(i2c: I2C, pagenb: int):
  """Set the page in MR11"""
  mr11 = pagenb >> 1
  mr11 = struct.pack("B", mr11)
  i2c.writeto_mem(I2C_ADDR, 0xb, mr11)

def spd5118write(i2c: I2C, offset: int, data: bytes) -> bytes:
  """Write the given data to the given offset, assumes all data fit within the
  same 64-byte block"""
  pagenb = offset // BLOCK_SIZE
  offset = offset % BLOCK_SIZE

  # First set the page in MR11
  spd5118setpage(i2c, pagenb)

  addr = (1 << 7) | ((pagenb & 1) << 6) | offset
  return i2c.writeto_mem(I2C_ADDR, addr, data)

def spd5118read(i2c: I2C, offset: int, nbbytes: int) -> bytes:
  """Read the given number of bytes from the given offset, assumes all data fit within the
  same 64-byte block"""
  pagenb = offset // BLOCK_SIZE
  offset = offset % BLOCK_SIZE

  # First set the page in MR11
  spd5118setpage(i2c, pagenb)

  # Read the entire page
  addr = (1 << 7) | ((pagenb & 1) << 6) | offset
  return i2c.readfrom_mem(I2C_ADDR, addr, nbbytes)
