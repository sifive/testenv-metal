"""Header file generators"""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=too-many-branches
#pylint: disable-msg=too-many-statements
#pylint: disable-msg=too-few-public-methods
#pylint: disable-msg=cell-var-from-loop

from copy import copy
from importlib import import_module
from logging import getLogger
from os import pardir
from os.path import basename, dirname, join as joinpath, realpath, splitext
from . import pprint
from re import match as re_match
from sys import modules, stderr
from textwrap import dedent
from time import gmtime
from typing import (Dict, Iterable, List, Optional, TextIO, Tuple, Type, Union)
try:
    from jinja2 import Environment as JiEnv
except ImportError:
    JiEnv = None
from .. import classproperty
from .devices import Devices
from .model import (OMAccess, OMDeviceMap, OMMemoryRegion, OMRegField,
                    OMRegStruct)


class OMHeaderGenerator:
    """Generic header generator (abstract class)

       :param regwidth: the defaut register width
       :param test: whether to generate additional sanity check code
       :param debug: whether to show traceback on error
    """

    ENABLED = False
    _DEVICE_GENERATORS = None

    def __init__(self, regwidth: int = 32, test: bool = False,
                 debug: bool = False):
        self._regwidth = regwidth
        self._test = test
        self._debug = debug
        self._log = getLogger('om.headgen')

    @classproperty
    def generators(cls) -> Dict[str, 'OMHeaderGenerator']:
        """Generate a map of supported generators."""
        generators = {}
        for name in dir(modules[__name__]):
            item = getattr(modules[__name__], name)
            if not isinstance(item, Type):
                continue
            if not issubclass(item, cls) or \
                    item == cls:
                continue
            if not item.ENABLED:
                getLogger('om.headergen').warning('Generator %s disabled', name)
                continue
            sname = name.replace('OM', '').replace('HeaderGenerator', '')
            generators[sname] = item
        return generators

    @classmethod
    def get_generator(cls, name: str) -> 'OMHeaderGenerator':
        """Get the generator for a specific device, or fallback to the
           default generator if none exists
        """
        log = getLogger('om.headgen')
        if cls._DEVICE_GENERATORS is None:
            # this generator does not support device-specific generators
            log.debug('No device-specific generators for %s', cls.__name__)
            return cls
        if not cls._DEVICE_GENERATORS:
            # this generator supports device-specific generators, but they
            # are not loaded yet
            for modname in Devices.modules:
                devmod = import_module(modname)
                for iname in dir(devmod):
                    item = getattr(devmod, iname)
                    if not isinstance(item, Type):
                        continue
                    if not issubclass(item, cls) or item == cls:
                        continue
                    # print(f'{item} is a subclass of {cls}')
                    prefix = cls.__name__.replace('HeaderGenerator', '')
                    sname = iname.replace(prefix, '')
                    sname = sname.replace('HeaderGenerator', '').lower()
                    cls._DEVICE_GENERATORS[sname] = item
                    log.info('Registered generator %s for device %s',
                             item.__name__, sname)
        try:
            name = name.lower()
            gen = cls._DEVICE_GENERATORS[name.lower()]
            log.debug('Use %s generator for device %s', gen.__name__, name)
            return gen
        except KeyError:
            log.debug('Use default generator for device %s', name)
            return cls

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


class OMSifiveSisHeaderGenerator(OMHeaderGenerator):
    """SiFive SIS header generator.
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

    HAS_TRAILING_DESC = True
    """Comments may be appended to the current line (vs. prefixing it)."""

    _DEVICE_GENERATORS = {}
    """This generator supports device-specific generators."""

    def generate_device(self, ofp: TextIO, device: OMDeviceMap,
                        bitsize: int) -> None:
        """Generate a SIS header file stream for a device.

           :param ofp: the output stream
           :param device: the device for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        env = JiEnv(trim_blocks=False)
        jinja = self.get_template('device')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        grpfields = device.fields
        groups = self.split_registers(grpfields, bitsize)
        devname = device.name
        descs = device.descriptors
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{device.name}.h'

        cgroups, regwidths = self._generate_device(devname, descs, groups,
                                                   bitsize)
        fgroups = self._generate_fields(devname, descs, groups, regwidths)
        bgroups = self._generate_bits(devname, groups, regwidths)
        dgroups = self._generate_definitions(devname, device.features)
        tgroups = {name: regwidths[name.lower()] for name in bgroups.keys()}
        enable_assertion = self._test
        ucomp = devname.upper()
        cyear = self.build_year_string()

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)

    @classmethod
    def get_template(cls, template: str) -> str:
        """Provide the path to the Jinja template file.

           :param template: the template kind
           :return: the pathname to the Jinja file.
        """
        return realpath(joinpath(dirname(__file__), pardir,
                        'templates', f'{template}.j2'))

    @classmethod
    def get_reserved_group_info(cls, offset: int, hwx: int) -> Tuple[str, str]:
        """Provide the comment string to generate for reserved/padding regs.

           :param offset: the offset of the register
           :param hwx: the count of hexa char to use for the offset
           :return: the permission string and the comment string
        """
        return '', ''

    def _generate_device(self,
            devname: str, descriptors: Dict[str, str],
            groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
            bitsize: int) -> Tuple[List[str], Dict[str, int]]:
        """Generate device structure as Jinja data.

           Output string are padded with space chars for proper alignments,
           as this feature is hard if not impossible to achieve with Jinja.

           :param devname: the device name
           :param descriptors: the register descriptors
           :param groups: the device registers
           :param int: maximum size in bits of registers
           :return: a 2-uple of:
                    * 4-item list of reg access mode, type, name and description
                    * a dictionay of reg name, reg size
        """
        # compute how many hex nibbles are required to encode all byte offsets
        lastgroup, _ = groups[list(groups.keys())[-1]]
        lastfield = lastgroup[list(lastgroup.keys())[-1]]
        hioffset = lastfield.offset//8
        encbit = int.bit_length(hioffset)
        hwx = (encbit + 3) // 4

        regwidth = self._regwidth
        regmask = regwidth - 1
        type_ = f'uint{regwidth}_t'

        cgroups: List[str] = []
        regsizes: Dict[str, int] = {'': bitsize}  # default value
        last_pos = 0
        rsv = 0
        for name, (group, repeat) in groups.items():
            # print(name)
            gdesc = descriptors.get(name, '')
            #if name == 'enables':
            #    print(fields)
            if isinstance(group, OMRegStruct):
                f_field = group.first_field
                l_field = group.last_field
            else:
                fields = list(group.values())
                f_field = fields[0]
                l_field = fields[-1]
            # cgroup generation
            try:
                padding = (f_field.offset & ~regmask) - last_pos
            except AttributeError:
                raise
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
                perm, desc = self.get_reserved_group_info(last_pos//8, hwx)
                cgroups.append([perm, type_, rname, desc])
                rsv += 1
            size = l_field.offset + l_field.size - f_field.offset
            tsize = (size + regmask)//regwidth
            # conditions to use 64 bit register
            # - 64 bit should be enable
            # - 32 bit word count should be even
            # - register should be aligned on a 64-bit boundary
            #   (assuming the structure is always aligned on 64-bit as well)
            if bitsize == 64 and (tsize & 1) == 0 and \
                    (f_field.offset & (bitsize - 1)) == 0:
                rtype = 'uint64_t'
                tsize >>= 1
                slotsize = bitsize
            else:
                rtype = type_
                slotsize = regwidth
            regsizes[name] = slotsize
            slotmask = slotsize - 1
            uname = name.upper()
            if repeat == 1:
                fmtname = f'{uname};' if tsize == 1 else f'{uname}[{tsize}U];'
            else:
                if tsize == 1:
                    fmtname = f'{uname}[{repeat}U];'
                else:
                    fmtname = f'{uname}[{tsize}U][{repeat}U];'
            access, perm = self.SIS_ACCESS_MAP[self.merge_access(fields)]
            offset = f_field.offset // 8
            desc = f'Offset: 0x{offset:0{hwx}X} ({access})'
            if gdesc:
                desc = f'{desc} {gdesc}'
            cgroups.append([perm, rtype, fmtname, desc])
            size = ((l_field.size + slotmask) & ~slotmask) * repeat
            last_pos = (fields[-1].offset + size) & ~slotmask

        # find the largest field of each cgroups column
        lengths = [0, 0, 0, 0]
        for cregs in cgroups:
            lengths = [max(a,len(b)) for a, b in zip(lengths, cregs)]

        # space filling in-place to cgroups columns
        widths = [l + self.EXTRA_SEP_COUNT for l in lengths]
        widths[-1] = None
        if not self.HAS_TRAILING_DESC:
            widths[-2] = None
        for cregs in cgroups:
            cregs[:] = self.pad_columns(cregs, widths)
        return cgroups, regsizes

    def _generate_definitions(self, devname: str,
            features: Dict[str, Dict[str, Union[bool, int]]]) -> Dict[str, int]:
        """Generate constant definitions.

           :param devname: the device name
           :param features:  map of supported features and subfeatures.
           :return: a map of definition name, values
        """
        # implementation is device specific
        return {}

    def _generate_fields(self,
            devname: str, descriptors: Dict[str, str],
            groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
            regwidths: Dict[str, int]) -> Dict[str, Tuple[List[str], str]]:
        """Generate register definitions as Jinja data.

           Output string are padded with space chars for proper alignments,
           as this feature is hard if not impossible to achieve with Jinja.

           :param devname: the device name
           :param descriptors: the register descriptors
           :param groups: the device registers
           :param regwidths: size in bits of each register
           :return: a map of register name, 2-uple of
                     * list of 3-item list of bf name, bf mask, bf desc,
                     * register description
        """
        ucomp = devname.upper()

        fgroups: Dict[str, Tuple[List[str], str]] = {}
        for name, (group, _) in groups.items():
            # fgroup generation
            base_offset = None
            fregisters = []
            bitsize = regwidths['']
            bitmask = bitsize - 1
            uname = name.upper()
            gdesc = descriptors.get(name, '')
            for fname, field in group.items():
                regwidth = regwidths[name]
                regmask = regwidth - 1
                ufname = fname.upper()
                if base_offset is None:
                    base_offset = field.offset & ~bitmask
                offset = field.offset - base_offset
                ffield = []
                fpos = offset & regmask
                fdesc = field.desc
                name_prefix = name.split('_', 1)[0]
                if fdesc.lower().startswith(name_prefix):
                    fdesc = fdesc[len(name_prefix):].lstrip()
                if fdesc:
                    fdesc = ''.join((fdesc[0].upper(), fdesc[1:]))
                fdesc = f'{gdesc}: {fdesc}'
                fieldname = f'{ucomp}_{uname}_{ufname}_Pos'
                ffield.append([fieldname, f'{fpos}U', fdesc])
                mask = (1 << field.size) - 1
                maskval = f'{mask}U' if mask < 9 else f'0x{mask:X}U'
                if field.size < regwidth:
                    if fpos or len(group) > 1:
                        maskval = f'({maskval} << {fieldname})'
                ffield.append([f'{ucomp}_{uname}_{ufname}_Msk', maskval, fdesc])
                fregisters.append(ffield)
            fgroups[uname] = (fregisters, gdesc)

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
        widths = (namelen+self.EXTRA_SEP_COUNT, None, None)
        for fregs, _ in fgroups.values():
            for freg in fregs:
                for ffield in freg:
                    ffield[:] = self.pad_columns(ffield, widths)
        return fgroups

    def _generate_bits(self,
            devname: str,
            groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
            regwidths: Dict[str, int]) \
        -> Dict[str, List[Tuple[str, List[str], List[str]]]]:
        """Generate register bitfield descriptions as Jinja data.
           Registers which are made of meaningful bitfields are described as
           integer bitfields.

           Output string are padded with space chars for proper alignments,
           as this feature is hard if not impossible to achieve with Jinja.

           :param devname: the device name
           :param groups: the device registers
           :param regwidths: size in bits of each register
           :return: a map of bitfield name, list of a 3-uple of
                     * the containing register type,
                     * a 3-item list of the bf type, the bf name, the bf desc
                     * a 2-item list of padding string for the bf and the reg.
        """
        def bitdesc(pos, size, desc):
            if size > 1:
                brange = f'{pos}..{pos+size-1}'
            else:
                brange = f'{pos}'
            return f'bit: {brange:>6s}  {desc}'

        def makebf(name: str, rewgwidth: int, pos: int, size: int) \
                -> Tuple[str, str]:
            for pwr in (8, 4, 2, 1):
                bsize = 8 * pwr
                # print(size, bsize)
                if size == bsize:
                    bmask = bsize - 1
                    if not (pos & bmask):
                        tstr = f'uint{bsize}_t'
                        vstr = f'{name};'
                        return tstr, vstr
            return f'uint{rewgwidth}_t', f'{name}:{size};'

        bgroups: Dict[str, List[Tuple[str, List[str], List[str]]]] = {}
        bflen = 0
        for name, (group, _) in groups.items():
            uname = name.upper()
            fieldcount = len(group)
            last_pos = 0
            rsv = 0
            bits = []
            base = None
            bitcount = 0
            regwidth = regwidths[name]
            regmask = regwidth - 1
            type_ = f'uint{regwidth}_t'
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
                    tstr, vstr = makebf(f'_reserved{rsv}', regwidth,
                                        last_pos, padding)
                    bits.append([tstr, vstr, desc])
                    rsv += 1
                    if len(vstr) > bflen:
                        bflen = len(vstr)
                desc = bitdesc(offset, field.size, field.desc)
                bitcount += field.size
                tstr, vstr = makebf(ufname, regwidth,
                                    offset, field.size)
                bits.append([tstr, vstr, desc])
                last_pos = offset + field.size
                if len(vstr) > bflen:
                    bflen = len(vstr)
            padding = regwidth - last_pos
            if padding > 0:
                bitcount += padding
                desc = bitdesc(last_pos, padding, '(reserved)')
                tstr, vstr = makebf(f'_reserved{rsv}', regwidth,
                                    last_pos, padding)
                bits.append([tstr, vstr, desc])
                if len(vstr) > bflen:
                    bflen = len(vstr)
            if bitcount > regwidth:
                self._log.warning('Fields for %s.%s.%s too wide: %d',
                                  devname, name, fname, bitcount)
                # there may be a better way to handle this, TBC
                continue
            if bits:
                bgroups[uname] = (type_, bits, [0, 0])
        widths = (len(type_), bflen + self.EXTRA_SEP_COUNT
                  if self.HAS_TRAILING_DESC else 0, 0)
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
        jinja = self.get_template('platform')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())

        memoryregions = []
        mlen = 0
        devtypes = {}
        addrw = addrsize//4  # two nibbles per byte
        for uniquename in memorymap:
            nmo = re_match(r'^(?P<dev>(?P<kind>[a-z]+)(?:\d+))(?:_.*)?',
                           uniquename)
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

    def generate_common_definitions(self, ofp: TextIO) -> None:
        """Generate a header file stream for the common definitions.

           :param ofp: the output stream
        """
        env = JiEnv(trim_blocks=False)
        jinja = self.get_template('definitions')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        cyear = self.build_year_string()
        enable_assertion = self._test
        text = template.render(locals())
        ofp.write(text)

    def generate_autotest(self, ofp: TextIO, header_files: List[str]) -> None:
        """Generate a C file stream to include all generared files.

           :param ofp: the output stream
           :param header_files: the file names to include
        """
        env = JiEnv(trim_blocks=False)
        jinja = self.get_template('autotest')
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


class OMMetalSisHeaderGenerator(OMSifiveSisHeaderGenerator):
    """SiFive SIS header generator for Metal.

       Same output which try to limit line to 80 char columns.
    """

    HAS_TRAILING_DESC = False

    @classmethod
    def get_template(cls, template: str) -> str:
        """Provide the path to the Jinja template file."""
        if template in ('device', 'definitions'):
            template = f'{template}_narrow'
        return joinpath(dirname(__file__), 'templates', f'{template}.j2')

    @classmethod
    def get_reserved_group_info(cls, offset: int, hwx: int) -> Tuple[str, str]:
        return cls.SIS_ACCESS_MAP[OMAccess.R][1], f'Offset: 0x{offset:0{hwx}X}'

