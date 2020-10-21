#!/usr/bin/env python3

"""Quick and dirty script to add source info to QEMU in_asm output.

   Dependencies: llvm-dwarf is required (part of LLVM/clang toolchain)
                 ELF should be built/linked with debug symbols (-g)
   Typical Usage:
      qemu-system-riscv64 -kernel test.elf -d in_asm 2>&1 | elf2qemu.py test.elf
"""

from argparse import ArgumentParser, FileType
from bisect import bisect
from os.path import isfile, join as joinpath, split
from re import compile as re_compile
from subprocess import run
from sys import argv, exit as sysexit, modules, stdin, stderr
from typing import TextIO


class Elf2Qemu:
    """Augment QEMU "in_asm" debug output with function/file/line information
       from executed ELF file.

       :param elfname: path to the executed ELF file.
       :param path: optional path to the LLVM bin directory
    """

    ELF_CRE = re_compile(r'(?i)^([\da-f]+)\s(?:([\da-f]+)\s)?(\w)\s(.*)$')
    QEMU_CRE = re_compile(r'(?i)^0x([\da-f]+): ')

    def __init__(self, elfname, path=None):
        self._elfname = elfname
        self._symbols = {}
        self._addresses = []
        self._source_cache = {}
        self._nm = joinpath(path or '', 'llvm-nm')
        self._dwarf = joinpath(path or '', 'llvm-dwarfdump')
        if not isfile(self._nm):
            raise ValueError(f'Unable to locate {self._nm}')
        if not isfile(self._dwarf):
            raise ValueError(f'Unable to locate {self._dwarf}')

    def list_symbols(self) -> None:
        """Load symbol address cache from ELF sections.
        """
        proc = run((self._nm, '-S', self._elfname),
                   capture_output=True, check=True, text=True)
        syms = []
        for line in proc.stdout.split('\n'):
            line = line.strip()
            emo = self.ELF_CRE.match(line)
            if not emo:
                continue
            if emo.group(3) not in 'tTwW':
                continue
            if not emo.group(2):
                continue
            start = int(emo.group(1), 16)
            size = int(emo.group(2), 16)
            name = emo.group(4)
            self._symbols[start] = (name, size)
            syms.append(start)
        self._addresses = sorted(syms)

    def find(self, address: int) -> int:
        """Find the closest known address in ELF DWARF info for a given PC.

           :param address: address to look for
           :return: closes address defined in ELF information.
        """
        pos = bisect(self._addresses, address)
        if pos:
            # print(f'{address:08x} -> {self._addresses[pos-1]:08x}')
            return self._addresses[pos-1]
        raise ValueError(f'0x{address:x}')

    def augment(self, qfp: TextIO) -> None:
        """Read lines from QEMU in_asm debug stream, and augment it with
           source information if any. Output to stdout.

           :param qfp: QEMU input text stream
        """
        match_last = False
        for line in qfp:
            line = line.strip()
            if not line:
                match_last = False
            qmo = self.QEMU_CRE.search(line)
            if not qmo or match_last:
                print(line)
                continue
            addr = int(qmo.group(1), 16)
            try:
                sym_addr = self.find(addr)
            except ValueError:
                print(line)
                continue
            name, size = self._symbols[sym_addr]
            distance = addr - sym_addr
            error = ' too large!' if distance > size else ''
            src_info = self.source(addr)
            print(f'>> {name} +{distance:x} {src_info}{error}')
            print(line)
            match_last = True

    def source(self, address: int) -> str:
        """Look for an address in the ELF file and extract file/line info out
           of it if possible.

           Use a cache to reduce call to llvm-dwarf.

           :param address: the PC address to decode
           :return: the info about the PC, in any
        """
        if address not in self._source_cache:
            proc = run((self._dwarf, '-lookup', hex(address), self._elfname),
                       capture_output=True, check=False, text=True)
            for line in proc.stdout.split('\n'):
                line = line.strip()
                if line.startswith('Line info:'):
                    line = line.split(':', 1)[1]
                    items = {k: v for k, v in [tuple(x.strip("'")
                        for x in items.strip().rsplit(' ', 1))
                        for items in line.split(',')]}
                    self._source_cache[address] = items['file'], items['line']
        return ':'.join(self._source_cache.get(address, ['?']))


def main() -> None:
    """Main routine"""
    debug = False
    try:
        module = modules[__name__]
        argparser = ArgumentParser(description=module.__doc__.split('\n')[0])

        argparser.add_argument('elf', nargs=1,
                               help='ELF file')
        argparser.add_argument('-p', '--path',
                               help='Path to LLVM bin directory')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args()
        debug = args.debug

        e2q = Elf2Qemu(args.elf[0], args.path)
        e2q.list_symbols()
        e2q.augment(stdin)

    except (IOError, OSError, ValueError) as exc:
        print('Error: %s' % exc, file=stderr)
        if debug:
            print_exc(chain=False, file=stderr)
        sysexit(1)
    except KeyboardInterrupt:
        sysexit(2)


if __name__ == '__main__':
    main()
