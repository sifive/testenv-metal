from copy import deepcopy
from os.path import commonprefix
from pprint import pprint
from typing import Dict, List, Tuple
from ..generator import OMSi5SisHeaderGenerator
from ..model import OMDeviceMap, OMRegField


class OMSi5SisPlicHeaderGenerator(OMSi5SisHeaderGenerator):
    """SiFive SIS header generator fpr PLIC interrupt controller.
    """

    DEVICE = 'plic'

    def __init__(self, regwidth: int = 32, test: bool = False,
                 debug: bool = False):
        super().__init__(regwidth, test, debug)

    def _transform_groups(self, device: OMDeviceMap,
                          groups: Dict[str, Tuple[Dict[str, OMRegField], int]],
                          bitsize: int) -> \
            Tuple[OMDeviceMap, Dict[str, Tuple[Dict[str, OMRegField], int]]]:
        newgroups = {}
        ffirst = None
        fcount = 0
        fdesc = ''
        for name, (group, repeat) in groups.items():
            # print(name)
            if name.startswith('priority'):
                if len(group) != 1:
                    raise ValueError('Unexpected priority field count')
                fname = list(group)[0]
                field = group[fname]
                if not ffirst:
                    ffirst = field
                    fcount = 1
                    fdesc = field.desc
                    continue
                # sanity check: all grouped register should be identical
                if (field.size != ffirst.size or
                    field.reset != ffirst.reset or
                    field.access != ffirst.access or
                    field.offset != ffirst.offset + fcount * 32):
                    raise ValueError('Unexpected priority deviation')
                fcount += 1
                fdesc = commonprefix((fdesc, field.desc))
                continue
            if fcount:
                greg = OMRegField(ffirst.offset, ffirst.size, fdesc.strip(),
                                  ffirst.reset, ffirst.access)
                newgroups['priority'] = ({'priority': greg}, fcount)
                ffirst = None
                fcount = 0
                fdesc = ''
            newgroups[name] = (group, repeat)
        # pprint(newgroups, sort_dicts=False)
        return device, newgroups
