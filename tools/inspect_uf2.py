#!/usr/bin/env python3
import struct
import sys
from collections import Counter

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

def inspect(path):
    addrs = []
    families = []
    sizes = []
    flags_seen = Counter()

    with open(path, "rb") as f:
        data = f.read()

    if len(data) % 512 != 0:
        print(f"[!] {path}: size is not multiple of 512: {len(data)} bytes")

    blocks = len(data) // 512

    for i in range(blocks):
        b = data[i*512:(i+1)*512]
        if len(b) < 512:
            continue

        magic0, magic1, flags, target, payload_size, block_no, num_blocks, family = struct.unpack_from("<IIIIIIII", b, 0)
        end_magic, = struct.unpack_from("<I", b, 512 - 4)

        if magic0 != UF2_MAGIC_START0 or magic1 != UF2_MAGIC_START1 or end_magic != UF2_MAGIC_END:
            continue

        addrs.append(target)
        families.append(family)
        sizes.append(payload_size)
        flags_seen[flags] += 1

    print(f"File: {path}")
    print(f"Size: {len(data)} bytes")
    print(f"UF2 blocks parsed: {len(addrs)} / {blocks}")

    if not addrs:
        print("[!] No valid UF2 blocks found")
        return

    print(f"Address min: 0x{min(addrs):08x}")
    print(f"Address max: 0x{max(addrs):08x}")
    print(f"Address span end approx: 0x{max(a+s for a, s in zip(addrs, sizes)):08x}")
    print(f"Payload sizes: {sorted(set(sizes))}")
    print(f"Families: {[hex(x) for x in sorted(set(families))]}")
    print(f"Flags: {[hex(x) for x in sorted(flags_seen)]}")
    print()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: inspect_uf2.py file.uf2 [file2.uf2 ...]")
        sys.exit(1)

    for p in sys.argv[1:]:
        inspect(p)
