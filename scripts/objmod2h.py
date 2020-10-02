#!/usr/bin/env python3

"""JSON Object Model to C header file generator."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop


from argparse import ArgumentParser, FileType
from enum import Enum
from json import loads as json_loads
from os.path import basename
from re import sub as re_sub
from sys import exit as sysexit, modules, stdin, stdout, stderr
from textwrap import dedent
from traceback import print_exc
from typing import (Any, DefaultDict, Dict, List, NamedTuple, Optional, TextIO,
                    Tuple)

Access = Enum('Access', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class RegField(NamedTuple):
    """Register description."""
    offset: int
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class ObjectModelConverter:
    """JSON object model converter.

       :param names: one or more IP names to extract from the object model
    """

    HEADER = """
    /**
     * %s registers
     * @file %s
     *
     * @copyright (c) 2020 SiFive, Inc
     * @copyright SPDX-License-Identifier: MIT
     */
    """

    EXTRA_SEP_COUNT = 2

    def __init__(self, *names: List[str]):
        self._comps: Dict[str,
                          Optional[Tuple[Dict[str, str],
                                         DefaultDict[str,
                                                     Dict[str, RegField]]]]] = \
            {x.lower(): None for x in names}

    def parse(self, mfp: TextIO) -> None:
        """Parse an object model text stream.

           :param mfp: object model file-like object
        """
        objmod = json_loads(mfp.read())
        if not isinstance(objmod, list):
            raise ValueError('Invalid format')
        objs = objmod[0]
        if not isinstance(objs, dict):
            raise ValueError('Invalid format')
        components = objs['components']
        if not isinstance(components, list):
            raise ValueError('Invalid format')
        for component in components:
            if 'memoryRegions' not in component:
                continue
            for region in component['memoryRegions']:
                if 'registerMap' not in region:
                    continue
                regname = region['name'].lower()
                if regname not in self._comps:
                    continue
                grpdescs, reggroups = self._parse_region(region)
                mreggroups = self._merge_registers(reggroups)
                self._comps[regname] = (grpdescs, mreggroups)

    def generate_header(self, ofp: TextIO, name: str, bitsize: int) -> None:
        """Generate a header file stream for a component.

           :param name: the name of a previously parsed component
           :param bitsize: the max width of register, in bits
           :param out: the output stream
        """
        try:
            grpdescs, grpfields = self._comps[name.lower()]
        except KeyError as exc:
            raise ValueError(f'Unknown component name: {name}') from exc
        newgroups = self._split_registers(grpfields, bitsize)
        prefix = name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{name}.h'
        self._generate_prolog(ofp, filename, prefix)
        self._generate_registers(ofp, prefix, newgroups, grpdescs)
        print('', file=ofp)
        self._generate_fields(ofp, bitsize, prefix, newgroups, grpdescs)
        self._generate_epilog(ofp, filename)

    @classmethod
    def _parse_region(cls, region: Dict[str, Any]) \
            -> Tuple[Dict[str, str], DefaultDict[str, Dict[str, RegField]]]:
        """Parse an object model region containing a register map.

            :param region: the region to parse
            :return: a 2-tuple of a group dictionay which gives the description
                     string for each named group, and a dictionary of groups of
                     register fields
        """
        regmap = region['registerMap']
        groups = {}
        for group in regmap["groups"]:
            name = group['name']
            desc = group.get('description', '').strip()
            if desc.lower().endswith(' register'):
                desc = desc[:-len(' register')]
            if name not in groups:
                groups[name] = desc
            elif not groups[name]:
                groups[name] = desc
        registers = DefaultDict(dict)
        for field in  regmap['registerFields']:
            bitbase = field['bitRange']['base']
            bitsize = field['bitRange']['size']
            regfield = field['description']
            name = regfield['name']
            if name == 'reserved':
                continue
            desc = regfield['description']
            group = regfield['group']
            reset = regfield.get('resetValue', None)
            access_ = list(filter(lambda x: x in Access.__members__,
                                  regfield['access'].get('_types', '')))
            access = Access[access_[0]] if access_ else None
            regfield = RegField(bitbase, bitsize, desc, reset, access)
            registers[group][name] = regfield
        return groups, registers

    @classmethod
    def _merge_registers(cls, reggroups: Dict[str, Dict[str, RegField]]) \
            -> Dict[str, Dict[str, RegField]]:
        """Merge 8-bit groups belonging to the same registers.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields
        """
        regnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        outregs = {}
        for gname in regnames:  # HW group name
            gregs = reggroups[gname] # HW register groups
            mregs = DefaultDict(list)
            # group registers of the same name radix into lists
            for fname in sorted(gregs, key=lambda n: gregs[n].offset):
                bfname = re_sub(r'_\d+', '', fname)  # register name w/o index
                mregs[bfname].append(gregs[fname])
            # merge siblings into a single register
            for mname in mregs:
                lregs = mregs[mname]
                if len(lregs) == 1:
                    # singleton
                    continue
                # pprint(mregs)
                breg = None
                nsize = None
                nreset = None
                nregs = []
                for lreg in lregs:
                    if breg:
                        #print(f'b.o {breg.offset} nz {nsize} l.o {lreg.offset}')
                        if lreg.offset == breg.offset+nsize:
                            if lreg.access != breg.access:
                                raise ValueError('Incoherent access modes')
                            nreset |= lreg.reset << nsize
                            nsize += lreg.size
                        else:
                            reg = RegField(breg.offset, nsize, breg.desc,
                                           nreset, breg.access)
                            nregs.append(reg)
                            breg = None
                    if breg is None:
                        breg = lreg
                        nsize = lreg.size
                        nreset = lreg.reset
                if breg:
                    reg = RegField(breg.offset, nsize, breg.desc,
                                   nreset, breg.access)
                    nregs.append(reg)
                # pprint(nregs)
                mregs[mname] = nregs
                # exit(0)
            outregs[gname] = mregs
        return outregs

    @classmethod
    def _split_registers(cls, reggroups: Dict[str, Dict[str, RegField]],
                         bitsize: int) -> Dict[str, Dict[str, RegField]]:
        """Split register fields into bitsized register words.

            :param reggroups: register fields (indexed on group, name)
            :param bitsize: max width of output registers
            :return: a dictionary of groups of register fields
        """
        outgroups = {}
        for gname in reggroups:
            registers = reggroups[gname]
            fregs = {}
            for regname in registers:
                fields = registers[regname]
                if len(fields) > 1:
                    # field with "holes"
                    raise NotImplementedError('Not handled')
                field = fields[0]
                if field.size <= bitsize:
                    fregs[regname] = field
                    continue
                mask = (1 << bitsize) - 1
                for idx in range(0, field.size, bitsize):
                    wsize = idx//bitsize
                    wreset = (field.reset >> idx) & mask
                    fregs[f'{regname}_{wsize}'] = RegField(
                        field.offset+idx, min(bitsize, field.size-idx),
                        f'{field.desc} {wsize}', wreset, field.access)
            outgroups[gname] = fregs
        return outgroups

    def _generate_registers(self, out: TextIO, prefix: str,
                            reggroups: Dict[str, Dict[str, RegField]],
                            groupdesc: Dict[str, str]):
        """Print out the register addresses (in bytes).

           :param out: output stream
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        grpnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        length = (len(prefix) + len('REGISTER') +
                 max([len(x) for x in reggroups]) + 2 + self.EXTRA_SEP_COUNT)
        for grpname in grpnames:
            group = reggroups[grpname]
            regname = sorted(group, key=lambda n: group[n].offset)[0]
            address = group[regname].offset//8
            addr_str = f'{prefix}_REGISTER_{grpname}'
            desc = groupdesc.get(grpname, '')
            print(f'/* {desc} */', file=out)
            print(f'#define {addr_str:{length}s} 0x{address:04X}u', file=out)

    def _generate_fields(self, out: TextIO, bitsize: int, prefix: str,
                         reggroups: Dict[str, Dict[str, RegField]],
                         groupdesc: Dict[str, str]):
        """Print out the register content (offset and mask).

           :param out: output stream
           :param bitsize: the max register width in bits
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        mask = (1 << bitsize) - 1
        grpnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        length = max([len(grpname) +
                      max([len(regname) for regname in reggroups[grpname]])
                          for grpname in reggroups])
        length += len(prefix) + len('REGISTER') + len('OFFSET') + 4
        length += self.EXTRA_SEP_COUNT
        for grpname in grpnames:
            group = reggroups[grpname]
            desc = groupdesc.get(grpname, '')
            print(f'/*\n * {desc}\n */', file=out)
            sorted_names = sorted(group, key=lambda n: group[n].offset)
            base_offset = group[sorted_names[0]].offset
            for regname in sorted_names:
                field = group[regname]
                offset = field.offset - base_offset
                mask = (1 << field.size) - 1
                print(f'/* {field.desc} [{field.access.name}] */', file=out)
                off_str = f'{prefix}_REGISTER_{grpname}_{regname}_OFFSET'
                msk_str = f'{prefix}_REGISTER_{grpname}_{regname}_MASK'
                padding = max(2, 2*((field.size+7)//8))
                print(f'#define {off_str:{length}s} {offset}u', file=out)
                print(f'#define {msk_str:{length}s} 0x{mask:0{padding}X}u',
                      file=out)
            print('', file=out)

    @classmethod
    def _generate_prolog(cls, out: TextIO, filename: str, name: str):
        """Generate file prolog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(dedent(cls.HEADER % (name, filename)).lstrip(), file=out)
        print(f'#ifndef {mult_ex}', file=out)
        print(f'#define {mult_ex}', file=out)
        print(file=out)

    @classmethod
    def _generate_epilog(cls, out: TextIO, filename: str):
        """Generate file epilog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(file=out)
        print(f'#endif /* {mult_ex} */', file=out)


def main() -> None:
    """Main routine"""
    debug = False
    try:
        argparser = ArgumentParser(description=modules[__name__].__doc__)

        argparser.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        argparser.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        argparser.add_argument('-w', '--width', type=int,
                               choices=(8, 16, 32, 64), default=32,
                               help='Output register width (default: 32)')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args()
        debug = args.debug

        omc = ObjectModelConverter('hca')
        omc.parse(args.input)
        omc.generate_header(args.output, 'hca', args.width)

    except (IOError, OSError, ValueError) as exc:
        print('Error: %s' % exc, file=stderr)
        if debug:
            print_exc(chain=False, file=stderr)
        sysexit(1)
    except SystemExit as exc:
        if debug:
            print_exc(chain=True, file=stderr)
        raise
    except KeyboardInterrupt:
        sysexit(2)


if __name__ == '__main__':
    main()
