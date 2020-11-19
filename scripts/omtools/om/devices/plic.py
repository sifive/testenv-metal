from collections import defaultdict
from copy import copy
from logging import getLogger
from os.path import basename #, dirname, join as joinpath, realpath, splitext
from re import match as re_match, sub as re_sub
from typing import Dict, List, Optional, TextIO, Tuple, Union
from .. import pprint
from ..misc import common_desc, flatten
from ..parser import OMDeviceParser
from ..generator import OMSifiveSisHeaderGenerator
from ..model import (HexInt, OMAccess, OMDeviceMap, OMInterrupt, OMNode,
                     OMRegField, OMRegStruct)
try:
    from jinja2 import Environment as JiEnv
except ImportError:
    JiEnv = None


class OMPlicDeviceParser(OMDeviceParser):
    """Specialized parser for PLIC devices."""

    def __init__(self, regwidth: int, debug: bool = False):
        super().__init__(regwidth, debug)
        self._log = getLogger('om.plicparser')

    def _build_device(self, node: OMNode, region: Dict) \
            -> Tuple[Dict[str, str],
                     Dict[str, Dict[str, bool]],
                     Dict[str,
                          Union[Tuple[OMRegField, int, int],
                                Tuple[OMRegStruct, int, int]]]]:
        grpdescs, freggroups = self._parse_region(region)
        freggroups, features = self._rework_fields(freggroups)
        return grpdescs, features, freggroups

    def _rework_fields(self, reggroups: Dict[str, Dict[str, OMRegField]]):
        ctx_counters = defaultdict(int)
        sub_regs = defaultdict(list)
        src_counters = set()
        radices = []
        sequences = []
        features = {}
        regmask = self._regwidth - 1
        for gname, gregs in reggroups.items():  # HW group name
            xmo = re_match(r'^(\w+?)(?:_(\d+))?$', gname)
            if not xmo:
                self._log.error('Unexpected name: %s', gname)
                return []
            radix = xmo.group(1)
            if radix not in radices:
                radices.append(radix)
                sequences.append([radix])
            else:
                if radix not in sequences[-1]:
                    seq_pos = None
                    for pos, seq in enumerate(reversed(sequences), start=1):
                        if seq[0] == radix:
                            seq_pos = pos
                            break
                    sequences, rem = sequences[:-seq_pos], sequences[-seq_pos:]
                    sequences.append(flatten(rem))
            if xmo.group(2) is not None:
                # enables_*, threshold_*, claim_complete_*
                ctx_counters[radix] += 1
                sub_regs[radix].append(gregs)
            else:
                # priority, pending
                src_counters.add(len(gregs))
        core_ctx = set(ctx_counters.values())
        if len(core_ctx) != 1:
            raise ValueError('Incoherent core sets')
        core_ctx_count = core_ctx.pop()
        if len(src_counters) != 1:
            raise ValueError('Incoherent source sets')
        irq_src_count = src_counters.pop()
        irq_wrd_count = (irq_src_count+regmask) // self._regwidth
        self._log.info('%d core contexts, %d (%d words) irq sources',
                       core_ctx_count, irq_src_count, irq_wrd_count)
        features['max_core_contexts'] = core_ctx_count
        features['max_int_sources'] = irq_src_count
        registers = {}
        for gname in radices:
            if gname in sub_regs:
                regs = []
                for grpreg in sub_regs[gname]:
                    if len(grpreg) > 1:
                        reg = self._fuse_registers(grpreg)
                        if reg is None:
                            self._log.error('Unable to fuse register %s', gname)
                            regs.clear()
                            break
                        regs.append(reg)
                if regs:
                    reg, stride, count = self._ext_factorize_registers(regs)
                else:
                    # only one entry in each sub_regs[gname]
                    for regdict in sub_regs[gname]:
                        regs.extend(regdict.values())
                    reg, stride, count = self._ext_factorize_registers(regs)
            else:
                regs = list(reggroups[gname].values())
                reg, stride, count = self._ext_factorize_registers(regs)
            registers[gname] = [reg, stride, count]
        newregs = {}
        for seq in sequences:
            strides = set()
            repeats = set()
            regs = []
            if len(seq) == 1:
                newregs[seq[0]] = tuple(registers[seq[0]])
            else:
                struct = OMRegStruct()
                for name in seq:
                    reg, stride, count = registers[name]
                    strides.add(stride)
                    repeats.add(count)
                    if regs:
                        offset = reg.offset - regs[-1].offset
                        if offset != self._regwidth:
                            raise ValueError('Padding not implemented')
                    struct[name] = reg, stride, count
                if len(strides) != 1 or len(repeats) != 1:
                    raise ValueError('Incoherent sequence')
                regname = ''.join(n[0] for n in seq)
                newregs[regname] = struct
        return newregs, features

    def _fuse_registers(self, regs: Dict[str, OMRegField]) -> OMRegField:
        fusereg = None
        for reg in regs.values():
            if fusereg:
                if reg.offset != fusereg.offset + fsize:
                    raise NotImplementedError('Hole detected')
                if reg.access != fusereg.access:
                    raise ValueError('Incoherent access modes')
                    if reg.reset is not None:
                        if freset is None:
                            freset = reg.reset << fsize
                        else:
                            freset |= reg.reset << fsize
                fsize += reg.size
                fdescs.append(reg.desc)
            else:
                fusereg = reg
                fsize = reg.size
                freset = reg.reset
                fdescs = [reg.desc]
        if fusereg:
            fdesc = self.common_description(fdescs)
            reg = OMRegField(HexInt(fusereg.offset), fsize, fdesc,
                             freset, fusereg.access)
            return reg
        return None

    def _ext_factorize_registers(self, regs: List[OMRegField]) -> \
            Tuple[OMRegField, int, int]:
        """Factorize similar, consecutive registers.

           :param regs: the list of registers to factorize
           :return: a 3-uple: the register, the stride (distance) in bits,
                              and count of repeated registers
        """
        basereg = None
        stride = None
        descs = []
        for pos, reg in enumerate(regs):
            if not basereg:
                basereg = reg
                descs.append(reg.desc)
                continue
            if reg.size != basereg.size:
                raise ValueError('Register size differ')
            if stride is None:
                stride = reg.offset - basereg.offset
            elif reg.offset - basereg.offset != stride * pos:
                raise ValueError('Register stride differ')
            if reg.reset != basereg.reset:
                raise ValueError('Register reset value differ')
            if reg.access != basereg.access:
                raise ValueError('Register access differ')
            descs.append(reg.desc)
        desc = self.common_description(descs)
        reg = OMRegField(basereg.offset, basereg.size, desc, basereg.reset,
                         basereg.access)
        if stride is None:
            # single register
            stride = self._regwidth
        return reg, HexInt(stride), len(regs)

    @classmethod
    def common_description(cls, descs: List[str]) -> str:
        fdescs = []
        for desc in descs:
            if not desc:
                continue
            parts = [x.strip() for x in desc.split('.')]
            fdescs.append(parts)
        parts = []
        for x in zip(*(fdescs)):
            x = list(x)
            common = x.pop()
            while x:
                common = common_desc(common, x.pop())
            parts.append(common)
        desc = '. '.join([p for p in parts if p])
        return desc


class OMSifiveSisPlicHeaderGenerator(OMSifiveSisHeaderGenerator):
    """SiFive SIS header generator for PLIC interrupt controller.
    """

    def __init__(self, regwidth: int = 32, test: bool = False,
                 debug: bool = False):
        super().__init__(regwidth, test, debug)
        self._rsv_field_count = 0

    def generate_device(self, ofp: TextIO, device: OMDeviceMap,
                        bitsize: int) -> None:
        """Generate a SIS header file stream for a device.

           :param ofp: the output stream
           :param device: the device for which to generate the file
           :param bitsize: the max width of register, in bits
        """
        env = JiEnv(trim_blocks=False)
        jinja = self.get_template('device_plic')
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{device.name}.h'

        groups = device.fields
        descs = device.descriptors
        dgroups = self._generate_definitions(device)
        sgroups = self._generate_fields(device)
        fgroups, bgroups = self._generate_bits(device)
        ucomp = device.name.upper()
        cyear = self.build_year_string()

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)

    def _generate_fields(self, device):
        """
        """
        fields = device.fields
        structures = {}
        main = []
        type_ = f'uint{self._regwidth}_t'
        pos = 0
        regmask = self._regwidth - 1
        ucomp = device.name.upper()
        lastfields = fields
        while True:
            lastfield = lastfields[list(lastfields.keys())[-1]]
            if not isinstance(lastfield, OMRegStruct):
                lastfield, _, _ = lastfield
                break
            lastfields = lastfield
        hioffset = lastfield.offset//8
        encbit = int.bit_length(hioffset)
        hwx = (encbit + 3) // 4

        repeat_str = {
            'priority': '1U+%s_MAX_INT_SOURCES' % ucomp,
            'enables': '%s_MAX_CORE_CONTEXTS' % ucomp,
            'tc':  '%s_MAX_CORE_CONTEXTS' % ucomp,
        }

        for name in fields:
            content = fields[name]
            if isinstance(content, OMRegStruct):
                stride, repeat = content.stride, content.repeat
                size = content.size
                uname = name.upper()
                ftype = f'{ucomp}_{uname}'
                repstr = repeat_str.get(name, f'{repeat}U')
                fmtname = f'{uname[:2]}_CTX[{repstr}];'
                strname = f'{name.upper()}[{word_count}U];'
                struct = []
                rsv = 0
                offset = None
                for regname in content:
                    reg = content[regname][0]
                    if offset is None:
                        offset = reg.offset
                    _, perm = self.SIS_ACCESS_MAP[reg.access]
                    desc = reg.desc.split('.')[0].strip()
                    struct.append([perm, type_, f'{regname.upper()};', ''])
                    padbits = stride-size
                struct.append(self._generate_field_padding(0, padbits,
                                                           hwx, rsv))
                rsv += 1
                structures[ftype] = struct
                ftype = f'{ftype}_Type'
                desc = ''
            else:
                reg, stride, repeat = content
                # include IRQ0 in generated arrays
                bitsize = reg.size
                if name == 'priority':
                    offset = reg.offset - self._regwidth
                    repeat += 1
                else:
                    offset = reg.offset - 1
                    bitsize += 1
                bitoffset = offset & regmask
                word_count = (bitoffset+bitsize+regmask) // self._regwidth
                stride_word = stride // self._regwidth
                word_size = (bitoffset+stride*repeat+regmask) // self._regwidth
                _, perm = self.SIS_ACCESS_MAP[reg.access]
                if stride_word > word_count:
                    uname = name.upper()
                    ftype = f'{ucomp}_{uname}'
                    repstr = repeat_str.get(name, f'{repeat}U')
                    fmtname = f'{uname[:2]}_CTX[{repstr}];'
                    padbits = stride-word_count*self._regwidth
                    strname = f'{name.upper()}[{word_count}U];'
                    struct = [[perm, type_, strname, reg.desc]]
                    struct.append(self._generate_field_padding(0, padbits, hwx,
                                                               0))
                    structures[ftype] = struct
                    ftype = f'{ftype}_Type'
                    desc = ''
                else:
                    ftype = type_
                    wsstr = repeat_str.get(name, f'{word_size}U')
                    fmtname = f'{name.upper()}[{wsstr}];'
                    desc = reg.desc
            if pos != offset:
                main.append(self._generate_field_padding(pos, offset-pos, hwx,
                                                         self._rsv_field_count))
                self._rsv_field_count += 1
            main.append([perm, ftype, fmtname, desc])
            pos = offset + (word_size * self._regwidth)
        structures[f'{ucomp}'] = main
        for struct in structures.values():
            lengths = [0] * len(struct[0])
            for cregs in struct:
                lengths = [max(a,len(b)) for a, b in zip(lengths, cregs)]
            widths = [l + self.EXTRA_SEP_COUNT for l in lengths]
            widths[-1] = None
            widths[-2] = None
            for cregs in struct:
                cregs[:] = self.pad_columns(cregs, widths)
        return structures

    def _generate_field_padding(self, pos: int, bitcount: int, hwx: int,
                                rsv: int):
        regmask = self._regwidth - 1
        word_count = (bitcount+regmask) // self._regwidth
        type_ = f'uint{self._regwidth}_t'
        rname = f'_reserved{rsv}'
        if word_count == 1:
            rname = f'{rname};'
        else:
            rname = f'{rname}[0x{word_count:x}U];'
        perm, desc = self.get_reserved_group_info(pos, hwx)
        return [perm, type_, rname, desc]

    def _make_bitfield(self, name: str, offset: int, size: int, desc: str):
        bfields = []
        regmask = self._regwidth - 1
        fpos = offset & regmask
        bfname = f'{name}_Pos'
        bfields.append([bfname, f'{fpos}U', desc])
        mask = (1 << size) - 1
        maskval = f'{mask}U' if mask < 9 else f'0x{mask:X}U'
        if size < self._regwidth:
            if fpos:
                maskval = f'({maskval} << {bfname})'
        bfields.append([f'{name}_Msk', maskval, desc])
        return bfields

    def _generate_bits(self, device):
        """
        """
        devname = device.name
        ucomp = devname.upper()
        fields = device.fields
        regwidths = {}
        bgroups = {}
        fgroups = {}
        regmask = self._regwidth - 1
        for fname, fval in fields.items():
            ufname = fname.upper()
            if not isinstance(fval, dict):
                reg, stride, repeat = fval
                if fname == 'priority':
                    bgroups[fname] = ({fname: reg}, repeat)
                    bfields = self._make_bitfield(
                        f'{ucomp}_{ufname}', reg.offset, reg.size, reg.desc)
                    fgroups[ufname] = ([bfields], '')
                    regwidths[fname] = self._regwidth
                    continue
                if fname in ('pending', 'enables'):
                    sgroups = {}
                    offset = reg.offset
                    base = offset & regmask
                    if fname == 'enables':
                        repeat = reg.size
                        desc = '.'.join(reg.desc.split('.')[1:])
                    else:
                        desc = reg.desc
                    bfields = []
                    for bit in range(base, repeat):
                        bdesc = f'IRQ {bit} {desc}'
                        wbit = bit & regmask
                        word = bit // self._regwidth
                        sgroups[f'irq{bit}'] = OMRegField(
                            HexInt(offset), 1, bdesc, reg.reset,
                            reg.access)
                        if wbit == regmask:
                            regname = f'{fname}_{word}'
                            regwidths[regname] = self._regwidth
                            bgroups[regname] = (sgroups, 1)
                            fgroups[regname.upper()] = (bfields, '')
                            sgroups = {}
                            bfields = []
                        offset += 1
                    if sgroups:
                        regname = f'{fname}_{word}'
                        regwidths[regname] = self._regwidth
                        bgroups[regname] = (sgroups, 1)
                        fgroups[regname.upper()] = ([], '')
                        fgroups[regname.upper()] = (bfields, '')
                    continue
            else:
                for sfname, sfval in fval.items():
                    reg, _, repeat = sfval
                    if reg.size == self._regwidth:
                        # full register width, not a bitfield
                        continue
                    usfname = sfname.upper()
                    ubname = f'{ufname}_{usfname}'
                    bgroups[ubname] = ({sfname: reg}, repeat)
                    bfields = self._make_bitfield(
                        f'{ucomp}_{ubname}', reg.offset, reg.size, reg.desc)
                    fgroups[ubname] = ([bfields], '')
                    regwidths[ubname] = self._regwidth
        bgroups = super()._generate_bits(devname, bgroups, regwidths)
        return fgroups, bgroups

    def _generate_definitions(self, device: OMDeviceMap) -> Dict[str, int]:
        """Generate constant definitions.

           :param device: the device
           :return: a map of definition name, values
        """
        # implementation is device specific
        devname = device.name
        features = device.features
        defs = {}
        uname = devname.upper()
        for feature, value in features.items():
            ufeat = feature.upper()
            defs[f'{uname}_{ufeat}'] = int(value or 0)
        pdefs = {}
        if not defs:
            return pdefs
        maxwidth = max([len(c) for c in defs]) + self.EXTRA_SEP_COUNT
        for name, value in defs.items():
            padlen = maxwidth - len(name)
            if padlen > 0:
                name = f"{name}{' '*padlen}"
            pdefs[name] = value
        return pdefs
