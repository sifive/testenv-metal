#!/usr/bin/env python3

from enum import Enum
from json import loads as json_loads
from pprint import pformat, pprint
from re import sub as re_sub
from typing import DefaultDict, List, NamedTuple, Optional


Access = Enum('Access', 'R RW W RFWT_ONE_TO_CLEAR')

class RegField(NamedTuple):
    offset: int
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class ObjectModelConverter:

    ACCESS_TYPES = ()

    def __init__(self, *names: List[str]):
        self._names = tuple((x.lower() for x in names))

    def parse(self, mfp):
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
                if region['name'].lower() not in self._names:
                    continue
                print(f'Found {region["name"]}')
                groups, reggroups = self._parse_region(region)
                mreggroups = self._merge_registers(reggroups)
                bitsize = 64
                newgroups = self._split_registers(mreggroups, bitsize)
                prefix = region['name'].upper()
                self._generate_registers(prefix, newgroups, bitsize)
                self._generate_fields(prefix, newgroups, bitsize)

    def _parse_region(self, region):
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

    def _merge_registers(self, reggroups):
        regnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        outregs = {}
        for gname in regnames:  # HW group name
            gregs = reggroups[gname] # HW register groups
            mregs = DefaultDict(list)
            # group registers of the same name radix into lists
            for fname in sorted(gregs, key=lambda n: gregs[n].offset):
                bfname = re_sub('_\d+', '', fname)  # register name w/o index
                mregs[bfname].append(gregs[fname])
            # merge siblings into a single register
            for mname in mregs:
                lregs = mregs[mname]
                if len(lregs) == 1:
                    # singleton
                    continue
                # pprint(mregs)
                breg = None
                noffset = None
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
                            print('NO match')
                            reg = RegField(breg.offset, nsize, breg.desc,
                                           nreset, bref.access)
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

    def _split_registers(self, reggroups, bitsize):
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
                        f'{field.desc} {wsize}', field.reset, field.access)
            outgroups[gname] = fregs
        # pprint(outgroups)
        return outgroups

    def _generate_registers(self, prefix: str, reggroups, bitsize):
        grpnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        for grpname in grpnames:
            group = reggroups[grpname]
            regname = sorted(group, key=lambda n: group[n].offset)[0]
            address = group[regname].offset//8
            addr_str = f'{prefix}_{grpname}'
            print (f'#define {addr_str:40s} 0x{address:04X}u')

    def _generate_fields(self, prefix: str, reggroups, bitsize):
        return
        mask = (1 << bitsize) - 1
        grpnames = tuple(sorted(reggroups, key=lambda n: min([r.offset
                                    for r in reggroups[n].values()])))
        length = max([len(grpname) +
                      max([len(regname) for regname in reggroups[grpname]])
                          for grpname in reggroups])
        length += len(prefix) + len('OFFSET') + 3
        length += 2  # extra spaces
        for grpname in grpnames:
            group = reggroups[grpname]
            for regname in sorted(group, key=lambda n: group[n].offset):
                field = group[regname]
                offset = field.offset % bitsize
                mask = (1 << field.size) - 1
                print(f'// {field.desc}')
                off_str = f'{prefix}_{grpname}_{regname}_OFFSET'
                msk_str = f'{prefix}_{grpname}_{regname}_MASK'
                padding = max(2, 2*((field.size+7)//8))
                print(f'#define {off_str:{length}s} {offset}u')
                print(f'#define {msk_str:{length}s} 0x{mask:0{padding}X}u')
                print()


def main(omfile):
    omc = ObjectModelConverter('HCA')
    with open(omfile, 'rt') as mfp:
        omc.parse(mfp)


if __name__ == '__main__':
    main('/Users/eblot/Downloads/hca.v0.6.0.objectModel.json')
