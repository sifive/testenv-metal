"""Header file generators"""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=too-many-branches
#pylint: disable-msg=too-many-statements
#pylint: disable-msg=too-few-public-methods
#pylint: disable-msg=cell-var-from-loop

from copy import copy
from os.path import basename, dirname, join as joinpath, splitext
from pprint import pprint
from re import match as re_match
from sys import modules, stderr
from textwrap import dedent
from time import gmtime
from typing import (Dict, Iterable, List, Optional, TextIO, Tuple, Type, Union)
try:
    from jinja2 import Environment as JiEnv
except ImportError:
    JiEnv = None
from .model import OMAccess, OMDeviceMap, OMMemoryRegion, OMRegField


class OMHeaderGenerator:
    """Generic header generator (abstract class)

       :param regwidth: the defaut register width
    """

    ENABLED = False

    def __init__(self, regwidth: int = 32, debug: bool = False):
        self._regwidth = regwidth
        self._debug = debug

    @classmethod
    def generators(cls) -> Dict[str, 'OMHeaderGenerator']:
        """Generate a map of supported generators."""
        generators = {}
        for name in dir(modules[__name__]):
            item = getattr(modules[__name__], name)
            if not isinstance(item, Type):
                continue
            if not issubclass(item, cls) or item == cls:
                continue
            if not item.ENABLED:
                print(f'no {name}')
                continue
            sname = name.replace('OM', '').replace('HeaderGenerator', '')
            generators[sname] = item
        return generators

    def generate_device(self, ofp: TextIO, device: OMDeviceMap,
                        bitsize: Optional[int] = None) -> None:
        """Generate a header file stream for a device.

           :param ofp: the output stream
           :param device: the device for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        raise NotImplementedError('Device generation is not supported with '
                                  'this generator')

    def generate_platform(self,
            ofp: TextIO,
            memorymap: Dict[str, OMMemoryRegion],
            intmap: Dict[str, Dict[str, int]],
            addrsize: int) -> None:
        """Generate a header file stream for the platform definitions.

           :param ofp: the output stream
           :param memory map: the memory map of the platform
           :param intmap: the interrupt map of the platform
           :param addrsize: the size in bits of the address bus
        """
        raise NotImplementedError('Device generation is not supported with '
                                  'this generator')

    def generate_definitions(self, ofp: TextIO) -> None:
        """Generate a header file stream for the common definitions.

           :param ofp: the output stream
        """
        raise NotImplementedError('Device generation is not supported with '
                                  'this generator')

    @classmethod
    def split_registers(cls,
        reggroups: Dict[str, Tuple[Dict[str, OMRegField], int]],
        bitsize: int) \
            -> Dict[str, Tuple[Dict[str, OMRegField], int]]:
        """Split register fields into bitsized register words.

           :param reggroups: register fields (indexed on group, name)
           :param bitsize: max width of output registers
           :return: a dictionary of groups of register fields
        """
        outgroups = {}
        for gname in reggroups:
            registers, repeat = reggroups[gname]
            fregs = {}
            for fname, field in registers.items():
                if field.size <= bitsize:
                    # default case, the field fits into a word register
                    fregs[fname] = field
                    continue
                # a bitfield is larger than a word register
                if field.offset & (bitsize - 1) != 0:
                    raise NotImplementedError(
                        f'Wide unaligned bitfield {gname}.{fname}: '
                        f'{field.offset}')
                mask = (1 << bitsize) - 1
                for idx in range(0, field.size, bitsize):
                    wsize = idx//bitsize
                    if field.reset is not None:
                        wreset = (field.reset >> idx) & mask
                    else:
                        wreset = None
                    fregs[f'{fname}_{wsize}'] = OMRegField(
                        field.offset+idx, min(bitsize, field.size-idx),
                        f'{field.desc} {wsize}', wreset, field.access)
            outgroups[gname] = (fregs, repeat)
        return outgroups


class OMLegacyHeaderGenerator(OMHeaderGenerator):
    """Legacy header generator.
    """

    ENABLED = True

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

    def generate_device(self, ofp: TextIO, device: OMDeviceMap,
                        bitsize: int) -> None:
        """Generate a header file stream for a device.

           :param ofp: the output stream
           :param device: the device for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        newgroups = self.split_registers(device.fields, bitsize)
        prefix = device.name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{device.name}.h'
        self._generate_prolog(ofp, filename, prefix)
        self._generate_features(ofp, prefix, device.features)
        self._generate_registers(ofp, prefix, newgroups, device.descriptors)
        print('', file=ofp)
        self._generate_fields(ofp, bitsize, prefix, newgroups,
                             device.descriptors)
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
                           features: Dict[str, Dict[str, Union[bool, int]]]) \
            -> None:
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
        reggroups: Dict[str, Dict[str, List[OMRegField]]],
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
        reggroups: Dict[str, Dict[str, List[OMRegField]]],
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
    """SiFive SIS headeer generator.
    """

    ENABLED = bool(JiEnv)

    EXTRA_SEP_COUNT = 2
    """Extra space count between columns."""

    SIS_ACCESS_MAP = {
        OMAccess.R: ('R/ ', '__IM'),
        OMAccess.RW: ('R/W', '__IOM'),
        OMAccess.W: (' /W', '__OM'),
        OMAccess.RFWT_ONE_TO_CLEAR: ('/WC', '__OM'),
    }
    """OMAccess map for SIS formats."""

    def generate_device(self, ofp: TextIO, device: OMDeviceMap,
                        bitsize: Optional[int] = None) -> None:
        """Generate a SIS header file stream for a device.

           :param ofp: the output stream
           :param device: the device for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        env = JiEnv(trim_blocks=False)
        jinja = joinpath(dirname(__file__), 'templates', 'device.j2')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        grpfields = device.fields
        groups = self.split_registers(grpfields, bitsize)
        ucomp = device.name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{device.name}.h'

        cgroups, fgroups = self._generate_1(device, groups, bitsize)
        bgroups = self._generate_2(device, groups, bitsize)
        cyear = self.build_year_string()

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)

    def _generate_1(self, device, groups, bitsize: int):
        # compute how many hex nibbles are required to encode all byte offsets
        grpfields = device.fields
        lastgroup, _ = grpfields[list(grpfields.keys())[-1]]
        lastfield = lastgroup[list(lastgroup.keys())[-1]]
        hioffset = lastfield.offset//8
        encbit = int.bit_length(hioffset)
        hwx = (encbit + 3) // 4

        regwidth = self._regwidth
        regmask = regwidth - 1
        type_ = f'uint{regwidth}_t'
        ucomp = device.name.upper()

        cgroups = {}
        fgroups = {}
        last_pos = 0
        rsv = 0
        for name, (group, repeat) in groups.items():
            gdesc = device.descriptors.get(name, '')
            fields = list(group.values())
            # cgroup generation
            padding = fields[0].offset-last_pos
            if padding >= regwidth:
                # padding bit space, defined as reserved words
                tsize = (padding + regmask)//regwidth
                rname = f'_reserved{rsv}'
                if tsize == 1:
                    rname = f'{rname};'
                elif tsize <= regwidth:
                    rname = f'{rname}[{tsize}U];'
                else:
                    rname = f'{rname}[0x{tsize:x}U];'
                cgroups[rname] = ['     ', type_, rname, '']
                rsv += 1
            size = fields[-1].offset + fields[-1].size - fields[0].offset
            tsize = (size + regmask)//regwidth
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
            uname = name.upper()
            if repeat == 1:
                fmtname = f'{uname};' if tsize == 1 else f'{uname}[{tsize}U];'
            else:
                if tsize == 1:
                    fmtname = f'{uname}[{repeat}U];'
                else:
                    fmtname = f'{uname}[{tsize}U][{repeat}U];'
            access, perm = self.SIS_ACCESS_MAP[self.merge_access(fields)]
            offset = fields[0].offset // 8
            desc = f'Offset: 0x{offset:0{hwx}X} ({access})'
            if gdesc:
                desc = f'{desc} {gdesc}'
            cgroups[uname] = [perm, rtype, fmtname, desc]
            regsize = (fields[-1].size + regmask) & ~regmask
            last_pos = fields[-1].offset + regsize

            # fgroup generation
            base_offset = None
            fregisters = []
            bitmask = bitsize - 1
            for fname, field in group.items():
                ufname = fname.upper()
                if base_offset is None:
                    base_offset = field.offset & ~bitmask
                offset = field.offset - base_offset
                ffield = []
                fpos = offset & bitmask
                fieldname = f'{ucomp}_{uname}_{ufname}_Pos'
                fdesc = field.desc
                name_prefix = name.split('_', 1)[0]
                if fdesc.startswith(name_prefix):
                    fdesc = fdesc[len(name_prefix):].lstrip()
                ffield.append([fieldname, f'{fpos}U',
                               f'{ucomp} {uname}: {fdesc} Position'])
                mask = (1 << field.size) - 1
                maskval = f'{mask}U' if mask < 9 else f'0x{mask:X}U'
                if field.size < bitsize:
                    maskval = f'({maskval} << {fieldname})'
                ffield.append([f'{ucomp}_{uname}_{ufname}_Msk', maskval,
                               f'{ucomp} {uname}: {fdesc} Mask'])
                fregisters.append(ffield)
            fgroups[uname] = (fregisters, gdesc)

        # find the largest field of each cgroups column
        lengths = [0, 0, 0, 0]
        for cregs in cgroups.values():
            lengths = [max(a,len(b)) for a,b in zip(lengths, cregs)]

        # space filling in-place to cgroups columns
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
        # space filling in-place to fgroups column
        widths = (namelen+self.EXTRA_SEP_COUNT,
                  vallen+self.EXTRA_SEP_COUNT,
                  None)
        for fregs, _ in fgroups.values():
            for freg in fregs:
                for ffield in freg:
                    ffield[:] = self.pad_columns(ffield, widths)
        return cgroups, fgroups

    def _generate_2(self, device, groups, bitsize: int):

        def bitdesc(pos, size, desc):
            if size > 1:
                brange = f'{pos}..{pos+size-1}'
            else:
                brange = f'{pos}'
            return f'bit: {brange:>6s}  {desc}'

        grpfields = device.fields
        regwidth = self._regwidth
        regmask = regwidth - 1
        type_ = f'uint{regwidth}_t'

        bgroups = {}
        bflen = 0
        for name, (group, _) in grpfields.items():
            uname = name.upper()
            fieldcount = len(group)
            last_pos = 0
            rsv = 0
            bits = []
            base = None
            bitcount = 0
            for fname, field in group.items():
                if fieldcount == 1:
                    if field.size >= regwidth:
                        last_pos = regwidth
                        break
                ufname = fname.upper()
                if field.size > regwidth:
                    raise RuntimeError(f'Field size too large: '
                                       f'{field.size}')
                if base is None:
                    base = field.offset & ~regmask
                offset = field.offset-base
                padding = offset - last_pos
                if padding > 0:
                    bitcount += padding
                    desc = bitdesc(last_pos, padding, '(reserved)')
                    vstr = f'_reserved{rsv}:{padding};'
                    bits.append([type_, vstr, desc])
                    rsv += 1
                    if len(vstr) > bflen:
                        bflen = len(vstr)
                desc = bitdesc(offset, field.size, field.desc)

                bitcount += field.size
                vstr = f'{ufname}:{field.size};'
                bits.append([type_, vstr, desc])
                last_pos = offset + field.size
                if len(vstr) > bflen:
                    bflen = len(vstr)
            padding = regwidth - last_pos
            if padding > 0:
                bitcount += padding
                desc = bitdesc(last_pos, padding, '(reserved)')
                vstr = f'_reserved{rsv}:{padding};'
                bits.append([type_, vstr, desc])
                if len(vstr) > bflen:
                    bflen = len(vstr)
            if bitcount > regwidth:
                if self._debug:
                    pprint(grpfields, stream=stderr)
                devname = device.name
                raise RuntimeError(f'Fields for {devname}.{name}.{fname} '
                                   f'too wide: {bitcount}')
            if bits:
                bgroups[uname] = (type_, bits, [0, 0])
        widths = (len(type_), bflen + self.EXTRA_SEP_COUNT, 0)
        swidth = sum(widths)
        for _, bitfield, padders in bgroups.values():
            bitfield[:] = [self.pad_columns(bits, widths) for bits in bitfield]
            bpad = ' ' * swidth
            wpad = ' ' * (swidth - 7)
            padders[:] = [bpad, wpad]
        return bgroups

    def generate_platform(self,
            ofp: TextIO,
            memorymap: Dict[str, OMMemoryRegion],
            intmap: Dict[str, Dict[str, int]],
            addrsize: int) -> None:
        """Generate a header file stream for the platform.

           :param ofp: the output stream
           :param memory map: the memory map of the platform
           :param intmap: the interrupt map of the platform
           :param addrsize: the size in bits of the address bus
        """
        env = JiEnv(trim_blocks=False)
        jinja = joinpath(dirname(__file__), 'templates', 'platform.j2')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())

        memoryregions = []
        mlen = 0
        devtypes = {}
        addrw = addrsize//4  # two nibbles per byte
        for uniquename in memorymap:
            nmo = re_match(r'^(?P<dev>(?P<kind>[a-z]+)(?:\d+))(?:_.*)?', uniquename)
            if not nmo:
                raise RuntimeError(f'Unexpected device name {uniquename}')
            kind = nmo.group('kind')
            if kind not in devtypes:
                devtypes[kind] = set()
            devtypes[kind].add(nmo.group('dev'))
        for name, memregion in memorymap.items():
            ucomp = name.upper()
            properties = []
            aname = f'{ucomp}_BASE_ADDRESS'
            properties.append([aname, f'0x{memregion.base:0{addrw}X}'])
            if len(aname) > mlen:
                mlen = len(aname)
            sname = f'{ucomp}_SIZE'
            properties.append([sname, f'0x{memregion.size:0{addrw}X}'])
            if len(sname) > mlen:
                mlen = len(sname)
            desc = f'{name.title()} {memregion.desc}'
            memoryregions.append((desc, properties))
        widths = (mlen + self.EXTRA_SEP_COUNT, None)
        for _, memregs in memoryregions:
            for memreg in memregs:
                memreg[:] = self.pad_columns(memreg, widths)

        intdomains = []
        ilen = 0
        for parent, imap in intmap.items():
            interrupts = []
            for name, ints in imap.items():
                ucomp = name.upper()
                for interrupt in ints:
                    iname = f'{ucomp}_INTERRUPT_{interrupt.name.upper()}_NUM'
                    if len(iname) > ilen:
                        ilen = len(iname)
                    interrupts.append([iname, interrupt.channel])
            intdomains.append(interrupts)
        widths = (ilen + self.EXTRA_SEP_COUNT, None)
        for interrupts in intdomains:
            for idesc in interrupts:
                idesc[:] = self.pad_columns(idesc, widths)
        cyear = self.build_year_string()

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)

    def generate_definitions(self, ofp: TextIO) -> None:
        """Generate a header file stream for the common definitions.

           :param ofp: the output stream
        """
        env = JiEnv(trim_blocks=False)
        jinja = joinpath(dirname(__file__), 'templates', 'definitions.j2')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        cyear = self.build_year_string()
        text = template.render(locals())
        ofp.write(text)

    def generate_autotest(self, ofp: TextIO, header_files: List[str]) -> None:
        """Generate a C file stream to include all generared files.

           :param ofp: the output stream
           :param header_files: the file names to include
        """
        env = JiEnv(trim_blocks=False)
        jinja = joinpath(dirname(__file__), 'templates', 'autotest.j2')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        cyear = self.build_year_string()
        filename = ofp.name
        text = template.render(locals())
        ofp.write(text)

    @classmethod
    def build_year_string(cls) -> str:
        year = gmtime().tm_year
        return f'2020-{year}' if year > 2020 else '2020'

    @classmethod
    def merge_access(cls, fields: List[OMRegField]) -> OMAccess:
        """Build the access of a register from its individual fields.

           :param fields: the fields to merge
           :return: the global access for the register
        """
        access = None
        for field in fields:
            if field.access == OMAccess.R:
                access = OMAccess.RW if access == OMAccess.W else OMAccess.R
            elif (field.access == OMAccess.W) or \
                 (field.access == OMAccess.RFWT_ONE_TO_CLEAR):
                access = OMAccess.RW if access == OMAccess.R else OMAccess.W
            elif field.access == OMAccess.RW:
                access = OMAccess.RW
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
