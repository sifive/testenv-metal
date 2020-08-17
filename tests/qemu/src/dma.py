#!/usr/bin/env python3

# Quick and dirty script to generate reference SHA values for unit tests

from binascii import hexlify
from hashlib import sha512
from struct import calcsize as scalc, pack as spack
from textwrap import fill

def main():
    length = 4*4096
    size = length//scalc('<I')
    chunk_size = min((size, 256))
    hash_ = sha512()
    for segment in range(0, size, chunk_size):
        chunk = list(range(segment, segment+chunk_size))
        buf = spack(f'<{chunk_size}I', *chunk)
        # print(hexlify(buf).decode())
        hash_.update(buf)
    print(hash_.hexdigest())
    carray = ', '.join([f'0x{byte:02X}' for byte in hash_.digest()])
    print(fill(carray, initial_indent=' '*4, subsequent_indent=' '*4))


if __name__ == '__main__':
    main()
