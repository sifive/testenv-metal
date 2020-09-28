#!/usr/bin/env python3

# Quick and dirty script to generate reference SHA values for unit tests

from binascii import hexlify
from hashlib import sha512
from struct import calcsize as scalc, pack as spack
from textwrap import fill

def print_c_array(name, data, align=None):
    if align is None:
        align = ''
    else:
        align = f'ALIGN({align}) '
    c_array = ', '.join([f'0x{byte:02X}' for byte in data])
    print(f'static const uint8_t _{name.upper()}[{len(data)}u] {align}= {{')
    print(fill(c_array, initial_indent=' '*4, subsequent_indent=' '*4,
               width=80))
    print(f'}};')
    print()


def main():
    length = 4*4096-32
    size = length//scalc('<I')
    chunk_size = min((size, 256))
    hash_ = sha512()
    print(size, chunk_size)
    for segment in range(0, size, chunk_size):
        chunk = list(range(segment, min((segment+chunk_size, size))))
        buf = spack(f'<{len(chunk)}I', *chunk)
        # print(hexlify(buf).decode())
        hash_.update(buf)
    print("//", hash_.hexdigest())
    print_c_array("long_buf_hash", hash_.digest());


if __name__ == '__main__':
    main()
