"""JSON Object Model parser."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=too-many-branches
#pylint: disable-msg=too-many-statements
#pylint: disable-msg=cell-var-from-loop

from json import loads as json_loads
from os.path import commonprefix
from pprint import pprint
from re import sub as re_sub
from sys import stderr
from typing import (Any, DefaultDict, Dict, Iterator, List, OrderedDict,
                    Optional, Sequence, TextIO, Tuple)
from .model import OMAccess, OMDevice, OMMemoryRegion, OMNode, OMRegField


class OMParser:
    """JSON object model converter.

       :param regwidth: the defaut register width
       :param debug: set to report more info on errors
    """

    def __init__(self, regwidth: int = 32, debug=False):
        self._regwidth = regwidth
        self._debug = debug
        self._devices: Dict[str, Optional[OMDevice]] = {}
        self._memorymap: OrderedDict[str, OMMemoryRegion] = {}
        self._irqmap: OrderedDict[str, int] = {}

    def get(self, name) -> OMDevice:
        """Return a device from its object model name, if any.

           :return: the parsed device
        """
        try:
            return self._devices[name]
        except KeyError as exc:
            raise ValueError(f"No '{name}' device in object model") from exc

    def __iter__(self) -> Iterator[OMDevice]:
        """Return an iterator for all parsed device, ordered by name.

           :return: a device iterator
        """
        for name in sorted(self._devices):
            yield self._devices[name]

    def parse(self, mfp: TextIO, names: Optional[List[str]] = None) -> None:
        """Parse an object model text stream.

           :param mfp: object model file-like object
           :param names: the device names to parse, or an empty list to
                         parse all devices.
        """
        objmod = json_loads(mfp.read())
        devmaps: List[Dict[str, OMMemoryRegion]] = []
        irqs: List[Dict[str, Tuple[str, int]]] = []
        for node in self._find_descriptors_of_type(objmod, 'OMDevice'):
            mmap, irqmap = self._parse_device(node, names or [])
            devmaps.extend(mmap)
            irqs.extend(irqmap)
        # pprint(devmaps)
        self._memorymap = self._merge_memory_regions(devmaps)
        self._irqmap = self._merge_interrupts(irqs)

    @classmethod
    def _find_descriptors_of_type(cls, root: Any, typename: str) \
            -> Iterator[OMNode]:
        for node in cls._find_kv_pair(root, '_types', typename):
            yield node

    @classmethod
    def _find_kv_pair(cls, node: Any, keyname: str, valname: str) \
            -> Iterator[OMNode]:
        if isinstance(node, dict):
            for key, value in node.items():
                if key == keyname:
                    if valname in value:
                        yield node
                for result in cls._find_kv_pair(value, keyname, valname):
                    yield result
        elif isinstance(node, list):
            for value in node:
                for result in cls._find_kv_pair(value, keyname, valname):
                    yield result

    def _parse_device(self, node: OMNode, names: List[str]) \
            -> Tuple[List[Dict[str, OMMemoryRegion]],
                     List[Dict[str, Tuple[str, int]]]]:
        """Parse a single device

           :param node: the device root to parse
           :param names: accepted device names (if empty, accept all)
        """
        mmaps: List[Dict[str, OMMemoryRegion]] = []
        irqs: List[Dict[str, Tuple[str, int]]] = []
        for region in node.get('memoryRegions', {}):
            if 'registerMap' not in region:
                continue
            regname = region['name'].lower()
            if names and regname not in names:
                continue
            width = node.get('beatBytes', 32)
            device = OMDevice(regname, width)
            grpdescs, reggroups = self._parse_region(region)
            features = self._parse_features(node)
            if width:
                features['data_bus'] = {'width': width}
            mmaps.append(self._parse_memory_map(node))
            irqs.append(self._parse_interrupts(node))
            freggroups = self._fuse_fields(reggroups)
            freggroups = self._factorize_fields(freggroups)
            device.descriptors = grpdescs
            device.fields = freggroups
            device.features = features
            self._devices[regname] = device
        return mmaps, irqs

    def _parse_region(self, region: OMNode) \
            -> Tuple[Dict[str, str],
                     OrderedDict[str, OrderedDict[str, List[OMRegField]]]]:
        """Parse an object model region containing a register map.

            :param region: the region to parse
            :return: a 2-tuple of a group dictionay which gives the description
                     string for each named group, and a dictionary of groups of
                     register fields
        """
        regmap = region['registerMap']
        groups = {}
        try:
            group = None
            for group in regmap['groups']:
                name = group['name']
                desc = group.get('description', '').strip()
                if desc.lower().endswith(' register'):
                    desc = desc[:-len(' register')]
                if name not in groups:
                    groups[name] = desc
                elif not groups[name]:
                    groups[name] = desc
        except Exception:
            if self._debug:
                pprint(group, stream=stderr)
            raise
        registers = DefaultDict(dict)
        try:
            field = None
            for field in  regmap['registerFields']:
                bitbase = field['bitRange']['base']
                bitsize = field['bitRange']['size']
                regfield = field['description']
                name = regfield['name']
                if name == 'reserved':
                    continue
                desc = regfield['description']
                # missing group?
                group = regfield.get('group', name)
                reset = regfield.get('resetValue', None)
                access_ = list(filter(lambda x: x in OMAccess.__members__,
                                      regfield['access'].get('_types', '')))
                access = OMAccess[access_[0]] if access_ else None
                regfield = OMRegField(bitbase, bitsize, desc, reset, access)
                registers[group][name] = regfield
        except Exception:
            if self._debug:
                pprint(field, stream=stderr)
            raise
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
    def _parse_features(cls, node: OMNode) -> Dict[str, Dict[str, bool]]:
        """Parse the supported feature of a device.

           :param node: the device node to parse
           :return: a dictionary of supported features, with optional options
        """
        features = {}
        for sectname in node:
            section = node[sectname]
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
    def _parse_interrupts(cls, node: OMNode) -> Dict[str, Tuple[str, int]]:
        """Parse interrupts definitions.

           :param node: the object model node to parse
           :return: an map of (name, (controller, channel))
        """
        ints = {}
        for section in node.get('interrupts', []):
            if not isinstance(section, dict) or '_types' not in section:
                continue
            types = section['_types']
            if 'OMInterrupt' not in types:
                continue
            name = section['name'].replace('@', '_').lower()
            channel = section['numberAtReceiver']
            parent = section['receiver']
            ints[name] = (parent, channel)
        return ints

    @classmethod
    def _parse_memory_map(cls, node: OMNode) -> Dict[str, OMMemoryRegion]:
        """Parse the memory map of a device.

           :param node: the object model compoent to parse
           :return:
        """
        mmap = {}
        for memregion in node.get('memoryRegions', []):
            for addrset in memregion.get('addressSets', []):
                name = memregion['name'].lower()
                mem = OMMemoryRegion(addrset['base'], addrset['mask'],
                                     memregion.get('description', ''))
            if name in mmap:
                raise ValueError(f'Memory region {name} redefined')
            mmap[name] = mem
        return mmap

    def _fuse_fields(self,
            reggroups: OrderedDict[str, OrderedDict[str, OMRegField]]) \
            -> OrderedDict[str, OrderedDict[str, OMRegField]]:
        """Merge 8-bit groups belonging to the same registers.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields
        """
        outregs = OrderedDict()
        for gname, gregs in reggroups.items():  # HW group name
            mregs = OrderedDict()
            # group registers of the same name radix into lists
            candidates = []
            for fname in sorted(gregs, key=lambda n: gregs[n].offset):
                bfname = re_sub(r'_\d+', '', fname)  # register name w/o index
                if not candidates or candidates[-1][0] != bfname:
                    candidates.append([bfname, []])
                candidates[-1][1].append(fname)
            for fusname, fldnames in candidates:
                if len(fldnames) == 1:
                    mregs[fldnames[0]] = gregs[fldnames[0]]
                    # nothing to fuse, single register
                    continue
                exp_pos = 0
                fusion = True
                for fname in fldnames:
                    reg = gregs[fname]
                    if exp_pos == 0 and reg.offset & (self._regwidth - 1) != 0:
                        fusion = False
                        # cannot fuse, first field does not start on a register
                        # boundary
                        break
                    if exp_pos and reg.offset != exp_pos:
                        fusion = False
                        # cannot fuse, empty space between field detected
                        break
                    exp_pos = reg.offset + reg.size
                if not fusion:
                    for fname in fldnames:
                        mregs[fname] = gregs[fname]
                    break
                # now attempt to fuse the fields, if possible
                mregs[fusname] = [gregs[fname] for fname in fldnames]
                for mname in mregs:
                    lregs = mregs[mname]
                    breg = None
                    nsize = None
                    nreset = None
                    for lreg in lregs:
                        if breg:
                            if lreg.offset == breg.offset+nsize:
                                if lreg.access != breg.access:
                                    raise ValueError('Incoherent access modes')
                                if lreg.reset is not None:
                                    if nreset is None:
                                        nreset = lreg.reset << nsize
                                    else:
                                        nreset |= lreg.reset << nsize
                                nsize += lreg.size
                            else:
                                reg = OMRegField(breg.offset, nsize, breg.desc,
                                                 nreset, breg.access)
                                nregs.append(reg)
                                breg = None
                        if breg is None:
                            breg = lreg
                            nsize = lreg.size
                            nreset = lreg.reset
                    if breg:
                        reg = OMRegField(breg.offset, nsize, breg.desc,
                                       nreset, breg.access)
                        mregs[mname] = reg
            outregs[gname] = mregs
        return outregs

    def _factorize_fields(self,
            reggroups: OrderedDict[str, OrderedDict[str, OMRegField]]) -> \
            OrderedDict[str, Tuple[OrderedDict[str, OMRegField], int]]:
        """Search if identical fields can be factorized.
           This implementation is quite fragile and should be reworked,
           as it only works with simple cases.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields and repeat count
        """
        outregs = OrderedDict()
        for gname, gregs in reggroups.items():  # HW group name
            factorize = True
            field0 = None
            stride = 0
            for field in gregs.values():
                if not field0:
                    field0 = field
                    stride = field0.offset & (self._regwidth -1)
                    continue
                if field.offset & (self._regwidth - 1) != stride:
                    factorize = False
                    break
                if (field.size != field0.size or
                    field.reset != field0.reset or
                    field.access != field0.access):
                    factorize = False
                    break
            if factorize:
                repeat = len(gregs)
                cname = commonprefix(list(gregs.keys()))
                cdesc = commonprefix([f.desc for f in gregs.values()])
                field = OMRegField(field0.offset, field0.size, cdesc,
                                 field0.reset, field0.access)
                greg = OrderedDict()
                greg[cname] = field
                outregs[gname] = (greg, repeat)
            else:
                outregs[gname] = (gregs, 1)
        return outregs

    @classmethod
    def _merge_memory_regions(cls,
        memregions: Sequence[Dict[str, OMMemoryRegion]]) \
            -> OrderedDict[str, OMMemoryRegion]:
        """Merge device memory map into a general platform map.

           This is a straightforware merge, which simply detects collision

           :param memregions: the device memory maps
           :return: the platform map
        """
        mmap = {}
        for devmap in memregions:
            for name, mreg in devmap.items():
                if name in mmap:
                    raise ValueError('Memory region {name} redefined')
                mmap[name] = mreg
        ommap = OrderedDict()
        last: Optional[Tuple[str, OMMemoryRegion]] = None
        for name in sorted(mmap, key=lambda n: mmap[n].base):
            reg = mmap[name]
            if last:
                if last[1].base + last[1].size > reg.base:
                    raise ValueError(f'Memory regions {last[0]} and {name} '
                                     f'overlap')
            ommap[name] = mmap[name]
            last = (name, mmap[name])
        return ommap

    @classmethod
    def _merge_interrupts(cls, irqs: Sequence[Dict[str, Tuple[str, int]]]) \
        -> OrderedDict[str, int]:
        """Merge interrup map into a general platform map.

           This is a straightforware merge, which simply detects collision

           :param irqs: the device interrupt maps
           :return: the platform map
        """
        imap = {}
        for irqmap in irqs:
            for name, irq in irqmap.items():
                if name in imap:
                    raise ValueError('Interrupt {name} redefined')
                imap[name] = irq
        oimap = OrderedDict()
        last: Optional[Tuple[str, Tuple[str, int]]] = None
        for name in sorted(imap, key=lambda n: imap[n]):
            irq = imap[name]
            if last:
                if last[1] == irq:
                    raise ValueError(f'IRQ {last[0]} and {name} overlap')
            oimap[name] = imap[name]
            last = (name, imap[name])
        return oimap
