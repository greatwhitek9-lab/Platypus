#!/usr/bin/env python3
import struct
import sys

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000

if len(sys.argv) != 4:
    print("Usage: patch_uf2_family.py input.uf2 output.uf2 0xFAMILYID")
    sys.exit(1)

inp = sys.argv[1]
outp = sys.argv[2]
new_family = int(sys.argv[3], 0)

with open(inp, "rb") as f:
    data = bytearray(f.read())

if len(data) % 512 != 0:
    raise SystemExit(f"Input size is not a multiple of 512: {len(data)}")

patched = 0
families_seen = set()

for off in range(0, len(data), 512):
    magic0, magic1, flags, target, payload_size, block_no, num_blocks, family = struct.unpack_from("<IIIIIIII", data, off)
    end_magic, = struct.unpack_from("<I", data, off + 508)

    if magic0 != UF2_MAGIC_START0 or magic1 != UF2_MAGIC_START1 or end_magic != UF2_MAGIC_END:
        continue

    families_seen.add(family)
    flags |= UF2_FLAG_FAMILY_ID_PRESENT
    struct.pack_into("<I", data, off + 8, flags)
    struct.pack_into("<I", data, off + 28, new_family)
    patched += 1

with open(outp, "wb") as f:
    f.write(data)

print(f"Input:          {inp}")
print(f"Output:         {outp}")
print(f"Blocks patched: {patched}")
print(f"Families seen:  {[hex(x) for x in sorted(families_seen)]}")
print(f"New family:     {hex(new_family)}")
