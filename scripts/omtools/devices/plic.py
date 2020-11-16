from copy import deepcopy
from difflib import SequenceMatcher
from pprint import pprint
from typing import Dict, List, Optional, Tuple
from ..generator import OMSi5SisHeaderGenerator
from ..model import HexInt, OMAccess, OMDeviceMap, OMRegField, OMRegStruct


class OMSi5SisPlicHeaderGenerator(OMSi5SisHeaderGenerator):
    """SiFive SIS header generator fpr PLIC interrupt controller.
    """

    DEVICE = 'plic'

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
                gdescs[gseq] = [self.common_desc(d, f.desc)
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

    @classmethod
    def common_desc(cls, desc1: str, desc2: str) -> str:
        """Find the common description from two description strings.

           :param desc1: first description string
           :param desc2: second description string
           :return: common description
        """
        matcher = SequenceMatcher(None, desc1, desc2)
        match = matcher.find_longest_match(0, len(desc1), 0, len(desc2))
        desc = desc1[match.a:match.a+match.size].strip()
        return desc

    def _post_priority_handler(self, blocknames, groups):
        # there is one register defined for each priority
        priority_count = groups[1]
        # enables requires one bit per priority
        enables_seqs = (priority_count+self._regwidth-1)//self._regwidth
        # count of successive enable registers to store all bits
        blocknames['enables'] = enables_seqs
