#!/usr/bin/env python3

"""JSON Object Model to C header file generator."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop


from argparse import ArgumentParser, FileType
from copy import copy
from enum import Enum
from json import loads as json_loads
from os.path import basename
from re import sub as re_sub
from sys import exit as sysexit, modules, stdin, stdout, stderr
from textwrap import dedent
from traceback import print_exc
from typing import (Any, DefaultDict, Dict, Iterable, Iterator, List,
                    NamedTuple, OrderedDict, Optional, TextIO, Tuple)
try:
    from jinja2 import Environment as JiEnv
except ImportError:
    JiEnv = None


from pprint import pformat, pprint

Access = Enum('Access', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class RegField(NamedTuple):
    """Register description.
    """
    offset: int
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class OMComponent:
    """Object model component container

       :param name: the component name, as defined in the object model
    """

    def __init__(self, name: str):
        self._name = name
        self._descriptors: Dict[str, str] = {}
        self._fields: OrderedDict[str, OrderedDict[str, RegField]] = {}
        self._features: Dict[str, Dict[str, bool]] = {}
        self._interrupts: OrderedDict[str, Tuple[str, int]] = {}

    @property
    def name(self) -> str:
        """Return the name of the component.

           :return: the name
        """
        return self._name

    @property
    def descriptors(self) -> Dict[str, str]:
        """Return a map of human readable strings for each defined register
           group.

           :return: a map of descriptor (register name, description)
        """
        return self._descriptors

    @property
    def fields(self) -> OrderedDict[str, OrderedDict[str, RegField]]:
        """Return an ordered map of registers.
           Each register is an ordered map of register fields.

           Order is the memory offset (from lowest to highest offsets)

           :return: a map of register field maps.
        """
        return self._fields

    @property
    def features(self) -> Dict[str, Dict[str, bool]]:
        """Return a map of supported features and subfeatures.

           :return: a map of subfeature maps.
        """
        return self._features

    @property
    def interrupts(self) -> OrderedDict[str, Tuple[str, int]]:
        """Return an ordered map of interrupt channels.

           Order is the channel number (from lowest to highest)

           :return: a map of (name, (controller, channel)).
        """
        return self._interrupts

    @descriptors.setter
    def descriptors(self, descs: Dict[str, str]) -> None:
        if not isinstance(descs, Dict):
            raise ValueError('Invalid descriptors')
        self._descriptors = descs

    @fields.setter
    def fields(self, fields: Dict[str, str]) -> None:
        if not isinstance(fields, OrderedDict):
            raise ValueError('Invalid fields')
        self._fields = fields

    @features.setter
    def features(self, features: Dict[str, str]) -> None:
        if not isinstance(features, Dict):
            raise ValueError('Invalid features')
        self._features = features

    @interrupts.setter
    def interrupts(self, interrupts: Dict[str, str]) -> None:
        if not isinstance(interrupts, OrderedDict):
            raise ValueError('Invalid interrupts')
        self._interrupts = interrupts


class OMHeaderGenerator:
    """Generic header generator (abstract class)
    """

    @classmethod
    def split_registers(cls,
        reggroups: OrderedDict[str, OrderedDict[str, RegField]],
        bitsize: int) -> OrderedDict[str, OrderedDict[str, RegField]]:
        """Split register fields into bitsized register words.

            :param reggroups: register fields (indexed on group, name)
            :param bitsize: max width of output registers
            :return: a dictionary of groups of register fields
        """
        outgroups = OrderedDict()
        for gname in reggroups:
            registers = reggroups[gname]
            fregs = OrderedDict()
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


class OMLegacyHeaderGenerator(OMHeaderGenerator):

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
    """Extra space count between columns."""

    def generate(self, ofp: TextIO, comp: OMComponent, bitsize: int) -> None:
        """Generate a header file stream for a component.

           :param ofp: the output stream
           :param comp: the component for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        newgroups = self.split_registers(comp.fields, bitsize)
        prefix = comp.name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{comp.name}.h'
        self._generate_prolog(ofp, filename, prefix)
        self._generate_features(ofp, prefix, comp.features)
        self._generate_registers(ofp, prefix, newgroups, comp.descriptors)
        print('', file=ofp)
        self._generate_fields(ofp, bitsize, prefix, newgroups,
                             comp.descriptors)
        self._generate_epilog(ofp, filename)

    @classmethod
    def _generate_prolog(cls, out: TextIO, filename: str, name: str) -> None:
        """Generate file prolog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(dedent(cls.HEADER % (name, filename)).lstrip(), file=out)
        print(f'#ifndef {mult_ex}', file=out)
        print(f'#define {mult_ex}', file=out)
        print(file=out)

    @classmethod
    def _generate_epilog(cls, out: TextIO, filename: str) -> None:
        """Generate file epilog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(file=out)
        print(f'#endif /* {mult_ex} */', file=out)

    @classmethod
    def _generate_features(cls, out: TextIO, prefix: str,
                           features: Dict[str, Dict[str, bool]]) -> None:
        """Generate the definitions of the supported features.

           :param out: output stream
           :param prefix: the prefix for all register names
           :param features: a dict of supported features and options
        """
        if not features:
            return
        print(f'/* Supported {prefix} features */', file=out)
        print('', file=out)
        lengths = [len(x) for x in features]
        lengths.extend([len(x)+len(feat)+1
                        for feat in features.values() for x in feat])
        length = len(prefix) + len(prefix) + 1 + max(lengths)
        length += cls.EXTRA_SEP_COUNT
        for featname in features:
            feat_str = f'{prefix}_HAS_{featname.upper()}'
            print(f'#define {feat_str:{length}s} 1', file=out)
            subfeatures = features[featname]
            for subname in subfeatures:
                val = int(subfeatures[subname])
                sub_str = f'{prefix}_HAS_{featname.upper()}_{subname.upper()}'
                print(f'#define {sub_str:{length}s} {val}', file=out)
            print('', file=out)
        print('', file=out)

    @classmethod
    def _generate_registers(cls,
        out: TextIO, prefix: str,
        reggroups: OrderedDict[str, OrderedDict[str, List[RegField]]],
        groupdesc: Dict[str, str]) -> None:
        """Print out the register addresses (in bytes).

           :param out: output stream
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        length = (len(prefix) + len('REGISTER') +
                 max([len(x) for x in reggroups]) + 2 + cls.EXTRA_SEP_COUNT)
        for grpname, registers in reggroups.items():
            firstfield = list(registers.values())[0]
            # print('FIRST', firstfield)
            address = firstfield.offset//8
            addr_str = f'{prefix}_REGISTER_{grpname}'
            desc = groupdesc.get(grpname, '')
            print(f'/* {desc} */', file=out)
            print(f'#define {addr_str:{length}s} 0x{address:04X}u', file=out)

    @classmethod
    def _generate_fields(cls,
        out: TextIO, bitsize: int, prefix: str,
        reggroups: OrderedDict[str, OrderedDict[str, List[RegField]]],
        groupdesc: Dict[str, str]) -> None:
        """Print out the register content (offset and mask).

           :param out: output stream
           :param bitsize: the max register width in bits
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        mask = (1 << bitsize) - 1
        length = max([len(grpname) +
                      max([len(regname) for regname in reggroups[grpname]])
                          for grpname in reggroups])
        length += len(prefix) + len('REGISTER') + len('OFFSET') + 4
        length += cls.EXTRA_SEP_COUNT
        for grpname, group in reggroups.items():
            desc = groupdesc.get(grpname, '')
            print(f'/*\n * {desc}\n */', file=out)
            base_offset = None
            for fname, field in group.items():
                if base_offset is None:
                    base_offset = field.offset
                    offset = 0
                else:
                    offset = field.offset - base_offset
                mask = (1 << field.size) - 1
                print(f'/* {field.desc} [{field.access.name}] */', file=out)
                off_str = f'{prefix}_REGISTER_{grpname}_{fname}_OFFSET'
                msk_str = f'{prefix}_REGISTER_{grpname}_{fname}_MASK'
                padding = max(2, 2*((field.size+7)//8))
                print(f'#define {off_str:{length}s} {offset}u', file=out)
                print(f'#define {msk_str:{length}s} 0x{mask:0{padding}X}u',
                      file=out)
            print('', file=out)


class OMSi5SisHeaderGenerator(OMHeaderGenerator):

    EXTRA_SEP_COUNT = 2
    """Extra space count between columns."""

    SIS_ACCESS_MAP = {
        Access.R: ('R/ ', '__IM'),
        Access.RW: ('R/W', '__IOM'),
        Access.W: (' /W', '__OM'),
        Access.RFWT_ONE_TO_CLEAR: ('/WC', '__OM'),
    }
    """Access map for SIS formats."""

    def generate(self, ofp: TextIO, comp: OMComponent, bitsize: int) -> None:
        """Generate a legacy header file stream for a component.

           :param out: the output stream
           :param comp: the component for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        env = JiEnv(trim_blocks=False)
        # template = env.from_string(modules[__name__].SIS_TEMPLATE.lstrip())
        from os.path import join as joinpath, splitext
        jinja = joinpath('.'.join((splitext(__file__)[0], 'j2')))
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        #     grpdescs, grpfields, features, ints = self._comps[name.lower()]
        grpfields = comp.fields
        groups = self.split_registers(grpfields, bitsize)
        ucomp = comp.name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{comp.name}.h'

        # constant that may be tweaked, or computed, TBD
        regbits = 32

        # comppute how many hex nibbles are required to encode all byte offsets
        lastgroup = grpfields[list(grpfields.keys())[-1]]
        lastfield = lastgroup[list(lastgroup.keys())[-1]][-1]
        hioffset = lastfield.offset//8
        encbit = int.bit_length(hioffset)
        hwx = (encbit + 3) // 4

        cgroups = OrderedDict()
        fgroups = OrderedDict()
        last_pos = 0
        rsv = 0
        type_ = f'uint{regbits}_t'

        for name, group in groups.items():
            gdesc = comp.descriptors.get(name, '')
            fields = list(group.values())
            # cgroup generation
            if last_pos:
                padding = fields[0].offset-last_pos
                if padding >= regbits:
                    tsize = (padding + regbits - 1)//regbits
                    rname = f'_reserved{rsv}'
                    rname = f'{rname};' if tsize == 1 else f'{rname}[{tsize}U];'
                    cgroups[rname] = ['     ', type_, rname, '']
                    rsv += 1
            size = fields[-1].offset + fields[-1].size - fields[0].offset
            tsize = (size + regbits - 1)//regbits
            # conditions to use 64 bit register
            # - 64 bit should be enable
            # - 32 bit word count should be even
            # - register should be aligned on a 64-bit boundary
            #   (assuming the structure is always aligned on 64-bit as well)
            if bitsize == 64 and (tsize & 1) == 0 and \
                    (fields[0].offset & (bitsize - 1)) == 0:
                rtype = 'uint64_t'
                tsize >>= 1
            else:
                rtype = type_
            fmtname = f'{name};' if tsize == 1 else f'{name}[{tsize}U];'
            access, perm = self.SIS_ACCESS_MAP[self.merge_access(fields)]
            offset = fields[0].offset // 8
            desc = f'Offset: 0x{offset:0{hwx}X} ({access}) {gdesc}'
            cgroups[name] = [perm, rtype, fmtname, desc]
            regsize = (fields[-1].size + regbits -1 ) & ~ (regbits - 1)
            last_pos = fields[-1].offset + regsize

            # fgroup generation
            base_offset = None
            fregisters = []
            for fname, field in group.items():
                if base_offset is None:
                    base_offset = field.offset
                    offset = 0
                else:
                    offset = field.offset - base_offset
                ffield = []
                fpos = offset & (bitsize - 1)
                fieldname = f'{ucomp}_{name}_{fname}_Pos'
                fdesc = field.desc
                name_prefix = name.split('_', 1)[0]
                if fdesc.startswith(name_prefix):
                    fdesc = fdesc[len(name_prefix):].lstrip()
                ffield.append([fieldname, f'{fpos}U',
                               f'{ucomp} {name}: {fdesc} Position'])
                mask = (1 << field.size) - 1
                maskval = f'{mask}U' if mask < 9 else f'0x{mask:X}U'
                if field.size < bitsize:
                    maskval = f'({maskval} << {fieldname})'
                ffield.append([f'{ucomp}_{name}_{fname}_Msk', maskval,
                               f'{ucomp} {name}: {fdesc} Mask'])
                fregisters.append(ffield)
            fgroups[name] = (fregisters, gdesc)

        # find the largest field of each cgroups column
        lengths = [0, 0, 0, 0]
        for cregs in cgroups.values():
            lengths = [max(a,len(b)) for a,b in zip(lengths, cregs)]

        # apply padding in-place to cgroups columns
        widths = [l + self.EXTRA_SEP_COUNT for l in lengths]
        widths[-1] = None
        for cregs in cgroups.values():
            cregs[:] = self.pad_columns(cregs, widths)

        # find the largest field of each fgroups column
        namelen = 0
        vallen = 0
        for fregs, _ in fgroups.values():
            for freg in fregs:
                for (name, val, _) in freg:
                    if len(name) > namelen:
                        namelen = len(name)
                    if len(val) > vallen:
                        vallen = len(val)
        # apply padding in-place to fgroups column
        widths = (namelen+self.EXTRA_SEP_COUNT,
                  vallen+self.EXTRA_SEP_COUNT,
                  None)
        for fregs, _ in fgroups.values():
            for freg in fregs:
                for ffield in freg:
                    ffield[:] = self.pad_columns(ffield, widths)

        def bitdesc(pos, size, desc):
            if size > 1:
                brange = f'{pos}..{pos+size-1}'
            else:
                brange = f'{pos}'
            return f'bit: {brange:>6s}  {desc}'

        bgroups = OrderedDict()
        bflen = 0
        for name, group in grpfields.items():
            fieldcount = len(group)
            if fieldcount <= 1:
                continue
            last_pos = 0
            rsv = 0
            bits = []
            base = None
            for fname, fields in group.items():
                for field in fields:
                    if base is None:
                        base = field.offset
                    offset = field.offset-base
                    padding = offset - last_pos
                    if padding > 0:
                        desc = bitdesc(last_pos, padding, 'Reserved')
                        vstr = f'_reserved{rsv}:{padding};'
                        bits.append([type_, vstr, desc])
                        rsv += 1
                        if len(vstr) > bflen:
                            bflen = len(vstr)
                    desc = bitdesc(offset, field.size, field.desc)
                    vstr = f'{fname}:{field.size};'
                    bits.append([type_, vstr, desc])
                    last_pos = offset + field.size
                    if len(vstr) > bflen:
                        bflen = len(vstr)
            padding = regbits - last_pos
            if padding > 0:
                desc = bitdesc(last_pos, padding, 'Reserved')
                vstr = f'_reserved{rsv}:{padding};'
                bits.append([type_, vstr, desc])
                if len(vstr) > bflen:
                    bflen = len(vstr)
            bgroups[name] = (type_, bits, [0, 0])
        widths = (len(type_), bflen + self.EXTRA_SEP_COUNT, 0)
        swidth = sum(widths)
        for _, bitfield, padders in bgroups.values():
            bitfield[:] = [self.pad_columns(bits, widths) for bits in bitfield]
            bpad = ' ' * swidth
            wpad = ' ' * (swidth - 7)
            padders[:] = [bpad, wpad]

        interrupts = []
        ilen = 0
        for name, (_, channel) in comp.interrupts.items():
            iname = f'{ucomp}_INTERRUPT_{name.upper()}_NUM'
            if len(iname) > ilen:
                ilen = len(iname)
            interrupts.append([iname, channel])
        widths = (ilen + self.EXTRA_SEP_COUNT, None)
        for idesc in interrupts:
            idesc[:] = self.pad_columns(idesc, widths)
        # cgroups = {}
        # fgroups = {}
        # bgroups = {}

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)

    @classmethod
    def merge_access(cls, fields: List[RegField]) -> Access:
        """Build the access of a register from its individual fields.

           :param fields: the fields to merge
           :return: the global access for the register
        """
        access = None
        for field in fields:
            if field.access == Access.R:
                access = Access.RW if access == Access.W else Access.R
            elif (field.access == Access.W) or \
                 (field.access == Access.RFWT_ONE_TO_CLEAR):
                access = Access.RW if access == Access.R else Access.W
            elif field.access == Access.RW:
                access = Access.RW
                return access
            else:
                raise ValueError('Invalid access')
        return access

    @classmethod
    def pad_columns(cls, columns: Iterable[str], widths: Iterable[int]) \
            -> Tuple[str]:
        """Append trailing space to each element of a list, to produce
           aligned printed columns.

           If a column width evaluates to false, the column is not padded.

           :param columns: strings to pad
           :param widths: width of each column.
        """
        outcols = []
        for col, width in zip(columns, widths):
            if not width:
                if not col:
                    outcols[-1] = outcols[-1].rstrip()
                outcols.append(col)
                continue
            colw = len(col)
            padlen = width - colw
            outcols.append(f"{col}{' '*padlen}" if padlen > 0 else col)
        return tuple(outcols)


class OMParser:
    """JSON object model converter.

       :param names: one or more IP names to extract from the object model
    """

    def __init__(self):
        self._components: Dict[str, Optional[OMComponent]] = {}

    def __getitem__(self, name) -> OMComponent:
        """Return a componen from its object model name, if any.

           :raise: KeyError if the component has not been parsed
           :return: the parsed component
        """
        comp = self._components[name]
        if comp is None:
            raise KeyError(f'{name} not parsed')
        return comp

    def __iter__(self) -> Iterator[OMComponent]:
        """Return an iterator for all parsed component, ordered by name.

           :return: a component iterator
        """
        names = sorted(self._components)
        for name in names:
            comp = self._components[name]
            if comp is None:
                continue
            yield comp

    def parse(self, mfp: TextIO, *names: str) -> None:
        """Parse an object model text stream.

           :param mfp: object model file-like object
        """
        names = list(*names)
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
                if regname not in names:
                    continue
                comp = OMComponent(regname)
                grpdescs, reggroups = self._parse_region(region)
                features = self._parse_features(component)
                mreggroups = self._merge_registers(reggroups)
                ints = self._parse_interrupts(component)
                comp.descriptors = grpdescs
                comp.fields = mreggroups
                comp.features = features
                comp.interrupts = ints
                self._components[regname] = comp

    @classmethod
    def _parse_region(cls, region: Dict[str, Any]) \
            -> Tuple[Dict[str, str],
                     OrderedDict[str, OrderedDict[str, List[RegField]]]]:
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
        foffsets = {}
        # sort the field by offset for each register group
        for grpname in registers:
            group = registers[grpname]
            fnames = sorted(group, key=lambda n: group[n].offset)
            fodict = OrderedDict(((name, group[name]) for name in fnames))
            foffsets[grpname] = group[fnames[0]].offset
            registers[grpname] = fodict
        godict = OrderedDict(((name, registers[name])
                             for name in sorted(registers,
                                                key=lambda n: foffsets[n])))
        return groups, godict

    @classmethod
    def _parse_features(cls, component: Dict[str, Any]) \
            -> Dict[str, Dict[str, bool]]:
        """Parse the supported feature of a component.cls

           :param component: the component to parse
           :return: a dictionary of supported features, with optional options
        """
        features = {}
        for sectname in component:
            section = component[sectname]
            if not isinstance(section, dict) or '_types' not in section:
                continue
            features[sectname] = {}
            for opt in section:
                if not opt.startswith('has'):
                    continue
                value = section[opt]
                if not isinstance(value, bool):
                    continue
                option = opt[3:]
                features[sectname][option] = value
        return features

    @classmethod
    def _parse_interrupts(cls, component: Dict[str, Any]) \
            -> OrderedDict[str, Tuple[str, int]]:
        """Parse interrupts definitions.

           :param component: the object model compoent to parse
           :return: an ordered map of (name, (controller, channel))
        """
        ints = {}
        for section in component.get('interrupts', []):
            if not isinstance(section, dict) or '_types' not in section:
                continue
            types = section['_types']
            if 'OMInterrupt' not in types:
                continue
            name = section['name']
            channel = section['numberAtReceiver']
            parent = section['receiver']
            ints[name] = (parent, channel)
        interrupts = OrderedDict(
            ((i, ints[i]) for i in sorted(ints, key=lambda i: ints[i])))
        return interrupts

    @classmethod
    def _merge_registers(cls,
        reggroups: OrderedDict[str, OrderedDict[str, RegField]]) \
            -> OrderedDict[str, OrderedDict[str, RegField]]:
        """Merge 8-bit groups belonging to the same registers.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields
        """
        outregs = OrderedDict()
        for gname, gregs in reggroups.items():  # HW group name
            mregs = OrderedDict()
            # group registers of the same name radix into lists
            for fname in sorted(gregs, key=lambda n: gregs[n].offset):
                bfname = re_sub(r'_\d+', '', fname)  # register name w/o index
                if bfname not in mregs:
                    mregs[bfname] = []
                mregs[bfname].append(gregs[fname])
            # merge siblings into a single register
            for mname in mregs:
                lregs = mregs[mname]
                if len(lregs) == 1:
                    continue
                breg = None
                nsize = None
                nreset = None
                nregs = []
                for lreg in lregs:
                    if breg:
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
                mregs[mname] = nregs
            outregs[gname] = mregs
        return outregs


SIS_TEMPLATE = """
/**
 * {{ ucomp }} registers
 * @file {{ filename }}
 *
 * @note This file has been automatically generated from the {{ucomp}} object model.
 *
 * @copyright (c) 2020 SiFive, Inc
 * @copyright SPDX-License-Identifier: MIT
 */

#ifndef SIFIVE_{{ ucomp }}_H_
#define SIFIVE_{{ ucomp }}_H_

/* following defines should be used for structure members */
#define __IM   volatile const   /**< Defines 'read only' structure member permissions */
#define __OM   volatile         /**< Defines 'write only' structure member permissions */
#define __IOM  volatile         /**< Defines 'read / write' structure member permissions */

typedef struct _{{ ucomp }} {
{%- for name, (perm, type_, name, desc) in cgroups.items() %}
    {{perm}} {{type_}} {{name}}{%- if desc -%}/**< {{desc}} */{%- endif -%}
{%- endfor %}
} {{ ucomp }}_Type;
{% for name, (group, gdesc) in fgroups.items() %}
{%- if name in bgroups %}
/**
 * Structure type to access {{gdesc}} ({{name}})
 */
{%- set type_, bitfield, padders = bgroups[name] %}
typedef union _{{ucomp}}_{{name}} {
    struct {
        {%- for (btype, bname, bdesc) in bitfield %}
        {{btype}} {{bname}} /**< {{bdesc}} */
        {%- endfor %}
    } b; {{padders[0]}} /**< Structure used for bit access */
    {{type_}} w; {{padders[1]}} /**< Structure used for word access */
} {{ucomp}}_{{name}}_Type;
{% endif %}
/* {{ucomp}} {% if gdesc %}{{gdesc}}{% endif %} */
    {%- for field in group %}
        {%- for name, value, desc in field %}
#define {{name}} {{value}} {% if desc %}/**< {{desc}} */{% endif -%}
        {% endfor %}
{% endfor -%}
{% endfor %}
/*
 * Interrupt channels from {{ucomp}}
 */
{% for name, channel in interrupts -%}
#define {{name}} {{channel}}U
{% endfor %}
#endif /* SIFIVE_{{ ucomp }}_H_ */
"""


def main(args=None) -> None:
    """Main routine"""
    debug = False
    outfmts = ['legacy']
    if JiEnv is not None:
        outfmts.append('si5sis')
    try:
        module = modules[__name__]
        argparser = ArgumentParser(description=module.__doc__)

        argparser.add_argument('comp', nargs=1,
                               help='Component(s) to extract from model')
        argparser.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        argparser.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        argparser.add_argument('-f', '--format', choices=outfmts,
                               default=outfmts[-1],
                               help=f'Output format (default: {outfmts[-1]})')
        argparser.add_argument('-w', '--width', type=int,
                               choices=(8, 16, 32, 64), default=64,
                               help='Output register width (default: 64)')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args(args)
        debug = args.debug

        omp = OMParser()
        omp.parse(args.input, args.comp)
        generator = getattr(module, f'OM{args.format.title()}HeaderGenerator')
        for comp in omp:
            generator().generate(args.output, comp, args.width)
            break

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
    from sys import argv
    if len(argv) > 1:
        main()
    else:
        main('-i /Users/eblot/Downloads/hcaDUT.objectModel.json -d -w 32 hca'.split())
