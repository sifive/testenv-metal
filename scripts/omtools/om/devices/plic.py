from collections import defaultdict
from copy import deepcopy
from logging import getLogger
from pprint import pprint
from re import match as re_match, sub as re_sub
from typing import Dict, List, Optional, Tuple, Union
from ..misc import common_desc, flatten
from ..parser import OMDeviceParser
from ..generator import OMSifiveSisHeaderGenerator
from ..model import (HexInt, OMAccess, OMDeviceMap, OMInterrupt, OMNode,
                     OMRegField, OMRegStruct)


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
        features = self._parse_features(node)
        freggroups = self._rework_fields(freggroups)
        return grpdescs, features, freggroups

    @classmethod
    def _parse_interrupts(cls, node: OMNode) -> List[OMInterrupt]:
        return []

    def _rework_fields(self, reggroups: Dict[str, Dict[str, OMRegField]]):
        #pprint(reggroups, sort_dicts=False)
        #exit(0)
        ctx_counters = defaultdict(int)
        sub_regs = defaultdict(list)
        src_counters = set()
        radices = []
        sequences = []
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
        # pprint(newregs, sort_dicts=False)
        return newregs

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
        desc = '. '.join(parts)
        return desc


class OMSifiveSisPlicHeaderGenerator(OMSifiveSisHeaderGenerator):
    """SiFive SIS header generator fpr PLIC interrupt controller.
    """

    def __init__(self, regwidth: int = 32, test: bool = False,
                 debug: bool = False):
        super().__init__(regwidth, test, debug)

    @classmethod
    def compare_regfield(cls, reg_a: OMRegField, reg_b: OMRegField,
                         stride: Optional[int] = None) -> bool:
        if reg_a.size != reg_b.size:
            print('size mismatch')
            return False
        if reg_a.reset != reg_b.reset:
            print('reset mismatch')
            return False
        if reg_a.access != reg_b.access:
            print('access mismatch')
            return False
        if stride is not None:
            if reg_a.offset + stride != reg_b.offset:
                print(f'stride mismatch {reg_a.offset} != '
                      f'0x{reg_b.offset + stride:x}')
                return False
        return True

    @classmethod
    def compare_regfield_group(cls,
            regs_a: List[OMRegField], regs_b: List[OMRegField],
            stride: Optional[int] = None) -> bool:
        if len(regs_a) != len(regs_b):
            return False
        for reg_a, reg_b in zip(regs_a, regs_b):
            if not cls.compare_regfield(reg_a, reg_b, stride):
                return False
        return True

    def _transform_groups(self, device: OMDeviceMap,
                          groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
                          bitsize: int) -> \
            Tuple[OMDeviceMap, Dict[str, Tuple[Dict[str, OMRegField], int]]]:
        newgroups = {}
        gfirsts = []
        seqcount = 0
        gdescs = []
        gseq = 0
        gcount = 0
        stride = None
        blocknames = {
            'priority': 1,
            'enables': 0
        }
        in_block = ''
        regsize = 32
        regmask = regsize-1
        for name, (group, repeat) in groups.items():
            D = name.startswith('enables')
            def debug(*args):
                if D:
                    print(*args)
            def pdebug(obj):
                if D:
                    pprint(obj, sort_dicts=False)
            if in_block:
                if not name.startswith(in_block):
                    if gcount:
                        if seqcount == 1:
                            ngroup = {f'{bname}_{idx}':
                                        OMRegField(f.offset, f.size, d,
                                                   f.reset, f.access)
                                      for idx, (f, d) in
                                        enumerate(zip(gfirsts[0], gdescs[0]))}
                            if len(ngroup) == 1:
                                ngroup = {bname: ngroup.popitem()[1]}
                            newgroups[bname] = (ngroup, gcount)
                        else:
                            struct = OMRegStruct()
                            spos = 0
                            for gfirst, gdesc in zip(gfirsts, gdescs):
                                ngroup = {}
                                for f, d in zip(gfirst, gdesc):
                                    reg = OMRegField(f.offset, f.size, d,
                                                     f.reset, f.access)
                                    ngroup[f'{bname}_{spos}'] = reg
                                    spos += 1
                                struct.append(ngroup)
                            first = gfirsts[0][0].offset & ~regmask
                            last = (gfirsts[-1][-1].offset + regmask) & ~regmask
                            gsize = last-first
                            padding = stride-gsize
                            if padding >= regsize:
                                preg = OMRegField(HexInt(gsize), padding, '',
                                                  None, OMAccess.R)
                                struct.append({'reserved': preg})
                            print(f'WITH STRIDE {stride:x}')
                            newgroups[bname] = (struct, gcount)
                        print(f'block commit w/ {bname}')
                        # pprint(newgroups[bname])
                        post_handler = getattr(self, f'_post_{bname}_handler',
                                               None)
                        if post_handler:
                            post_handler(blocknames, newgroups[bname])
                        pprint(blocknames)
                        gfirsts = []
                        gdescs = []
                        seqcount = 0
                        gseq = 0
                        gcount = 0
                        stride = None
                    in_block = None
            if not in_block:
                for bname in blocknames:
                    if name.startswith(bname):
                        debug(f'block enter w/ {bname}')
                        in_block = bname
                        seqcount = blocknames[bname]
                        if not seqcount:
                            raise RuntimeError(f'Invalid seq for {bname}')
                        break
                else:
                    debug(f'find no new block from {name}')
            if in_block:
                if len(gfirsts) < seqcount:
                    # build the first group
                    gfirst = list(group.values())
                    gfirsts.append(gfirst)
                    gdescs.append([f.desc for f in gfirst])
                    gcount = 1
                    gseq = 0
                    continue
                gregs = list(group.values())
                # sanity check: all grouped registers should be identical
                if not self.compare_regfield_group(
                    gfirsts[gseq], gregs, gcount * stride if stride else None):
                    raise ValueError(f'Unexpected {bname}:{gseq} deviation')
                gdescs[gseq] = [common_desc(d, f.desc)
                                for d, f in zip(gdescs[gseq], gregs)]
                gseq += 1
                if gseq == seqcount:
                    if gcount == 1:
                        first_field = gfirsts[-1][0]
                        field = gregs[0]
                        stride = field.offset - first_field.offset
                        print(f'STRIDE {bname} 0x{stride:x}')
                    gcount += 1
                    gseq = 0
                continue
            newgroups[name] = (group, repeat)
        print('INPUT--------------')
        pprint(groups['enables_0_0'], sort_dicts=False)
        print('OUTPUT-------------')
        pprint(newgroups['enables'], sort_dicts=False)
        print('-------------------')
        return device, newgroups

    def _post_priority_handler(self, blocknames, groups):
        # there is one register defined for each priority
        priority_count = groups[1]
        # enables requires one bit per priority
        enables_seqs = (priority_count+self._regwidth-1)//self._regwidth
        # count of successive enable registers to store all bits
        blocknames['enables'] = enables_seqs
