#!/usr/bin/env python3

from binascii import hexlify, unhexlify
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from os import urandom
from textwrap import fill

# example from hca_aes_128_GCM3

key = '48b7f337cdf9252687ecc760bd8ec184'
iv = '3e894ebb16ce82a53c3e05b2'
aad = ('7d924cfd37b3d046a96eb5e132042405'
       'c8731e06509787bbeb41f25827574649'
       '5e884d69871f77634c584bb007312234')
plain = ('bb2bac67a4709430c39c2eb9acfabc0d'
         '456c80d30aa1734e57997d548a8f0603')
ref_cipher = ('d263228b8ce051f67e9baf1ce7df97d1'
          '0cd5f3bc972362055130c7d13c3ab2e7')
ref_tag = '71446737ca1fa92e6d026d7d2ed1aa9c'


aesgcm = AESGCM(unhexlify(key))
ct = aesgcm.encrypt(unhexlify(iv), unhexlify(plain), unhexlify(aad))
cipher, tag = ct[:-16], ct[-16:]
# print(hexlify(cipher).decode())
# print(hexlify(tag).decode())
# check the lib produces what's expected
assert(ref_cipher == hexlify(cipher).decode())
assert(ref_tag == hexlify(tag).decode())


def print_c_array(name, data):
    c_array = ', '.join([f'0x{byte:02X}' for byte in data])
    print(f'static const uint8_t _{name.upper()}[{len(data)}u] = {{')
    print(fill(c_array, initial_indent=' '*4, subsequent_indent=' '*4,
               width=80))
    print(f'}};')
    print()

DMA_ALIGNMENT = 32
DMA_BLOCK_SIZE = 16

nkey = AESGCM.generate_key(bit_length=128)
niv = urandom(96//8)
# be sure to generate value that fit no blocks...
naad = urandom(DMA_ALIGNMENT+DMA_BLOCK_SIZE+9)
nplain = urandom(3*DMA_ALIGNMENT+DMA_BLOCK_SIZE+7)
nct = AESGCM(nkey).encrypt(niv, nplain, naad)
ncipher, ntag = nct[:-16], ct[-16:]

print_c_array('key_gcm2', nkey)
print_c_array('iv_gcm2', niv)
print_c_array('aad_gcm2', naad)
print_c_array('plaintext_gcm2', nplain)
print_c_array('ciphertext_gcm2', ncipher)
print_c_array('tag_gcm2', ntag)

