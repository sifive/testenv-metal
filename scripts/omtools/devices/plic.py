from copy import deepcopy
from os.path import commonprefix
from pprint import pprint
from typing import Dict, List, Optional, Tuple
from ..generator import OMSi5SisHeaderGenerator
from ..model import OMDeviceMap, OMRegField


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
        offset = 0
        for reg_a, reg_b in zip(regs_a, regs_b):
            offset += stride
            # pprint(reg_a)
            # pprint(reg_b)
            if not cls.compare_regfield(reg_a, reg_b, offset):
                return False
        return True

    def _transform_groups(self, device: OMDeviceMap,
                          groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
                          bitsize: int) -> \
            Tuple[OMDeviceMap, Dict[str, Tuple[Dict[str, OMRegField], int]]]:
        newgroups = {}
        gfirst = None
        gcount = 0
        gdescs = []
        blocknames = 'priority enables'.split()
        in_block = ''
        D = device.name == 'plic'
        def debug(*args):
            if D:
                print(*args)
        def pdebug(*args):
            if D:
                pprint(*args, sort_dicts=False)
        for name, (group, repeat) in groups.items():
            if in_block:
                if not name.startswith(in_block):
                    if gcount:
                        ngroup = [OMRegField(f.offset, f.size, d.strip(),
                                             f.reset, f.access)
                                  for f, d in zip(gfirst, gdesc)]
                        newgroups[bname] = ({bname: ngroup}, gcount)
                        debug(f'block commit w/ {bname}')
                        pdebug(newgroups[bname])
                        gfirst = None
                        gcount = 0
                        gdesc = []
                    in_block = None
            if not in_block:
                for bname in blocknames:
                    if name.startswith(bname):
                        debug(f'block enter w/ {bname}')
                        in_block = bname
                        break
                else:
                    debug(f'find no new block from {name}')
            if in_block:
                if len(group) != 1:
                    pdebug(group)
                #    raise ValueError(f'Unexpected {bname} field count: '
                #                     f'{len(group)}')
                fname = commonprefix(tuple(group)).rstrip('_')
                debug(f'fname {fname}')
                if not gfirst:
                    gfirst = list(group.values())
                    gcount = 1
                    gdesc = [f.desc for f in gfirst]
                    continue
                # sanity check: all grouped register should be identical
                gregs = list(group.values())
                if not self.compare_regfield_group(gfirst, gregs, gcount * 32):
                    raise ValueError(f'Unexpected {bname} deviation')
                gdesc = [commonprefix((d, f.desc))
                         for d, f in zip(gdesc, gregs)]
                gcount += 1
                continue
            newgroups[name] = (group, repeat)
        return device, newgroups
