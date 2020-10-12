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
from re import match as re_match, sub as re_sub
from sys import stderr
from typing import (Any, DefaultDict, Dict, Iterable, Iterator, List,
                    Optional, Sequence, Set, TextIO, Tuple)
from .model import (HexInt, OMAccess, OMDevice, OMInterrupt, OMMemoryRegion,
                    OMNode, OMRegField)


class OMParser:
    """JSON object model converter.

       :param regwidth: the defaut register width
       :param debug: set to report more info on errors
    """

    def __init__(self, regwidth: int = 32, debug: bool = False):
        self._regwidth = regwidth
        self._xlen: Optional[int] = None
        self._debug = debug
        self._devices: Dict[str, Optional[OMDevice]] = {}
        self._memorymap: Dict[str, OMMemoryRegion] = {}
        self._intmap: Dict[str, Dict[str, int]] = {}

    def get(self, name) -> Iterable[OMDevice]:
        """Return evice from its object model name, if any.

           :return: the parsed device
        """
        for dev in self._devices.values():
            if dev.name == name:
                yield dev

    @property
    def xlen(self) -> int:
        """Return the platform bus width."""
        return self._xlen

    @property
    def memory_map(self) -> Dict[str, OMMemoryRegion]:
        """Return the platform memory map."""
        return self._memorymap

    @property
    def interrupt_map(self) -> Dict[str, Dict[str, int]]:
        """Return the platform interrupt map."""
        return self._intmap

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
        devmaps: List[Tuple[str, Dict[str, OMMemoryRegion]]] = []
        interrupts: List[Tuple[str, List[OMInterrupt]]] = []
        devkinds = {}
        for path, node in self._find_descriptors_of_type(objmod, 'OMCore'):
            xlen = {node.get('isa').get('xLen')}
        self._xlen = xlen.pop()
        if xlen:
            raise ValueError(f'Too many xLen values: {xlen}')
        self._xlen = 64
        for path, node in self._find_descriptors_of_type(objmod, 'OMDevice'):
            dev = self._parse_device(path, node, names or [])
            if not dev:
                continue
            devname, mmaps, ints, devices = dev
            if devname not in devkinds:
                devkinds[devname] = 0
            else:
                devkinds[devname] += 1
            name = f'{devname}{devkinds[devname]}'
            if mmaps:
                devmaps.append((name, mmaps))
            if ints:
                interrupts.append((name, ints))
            # to be reworked
            if len(devices) == 1:
                self._devices[name] = list(devices.values())[0]
            else:
                for subname, device in devices.items():
                    self._devices[f'{name}_{subname}'] = device
        self._memorymap = self._merge_memory_regions(devmaps)
        self._intmap = self._merge_interrupts(interrupts)

    @classmethod
    def _find_descriptors_of_type(cls, root: Any, typename: str) \
            -> Iterator[Tuple[str, OMNode]]:
        for rpath, node in cls._find_kv_pair(root, '_types', typename):
            path = '.'.join(reversed(rpath))
            yield path, node

    @classmethod
    def _find_kv_pair(cls, node: Any, keyname: str, valname: str) \
            -> Iterator[Tuple[List[str], OMNode]]:
        if isinstance(node, dict):
            for key, value in node.items():
                if key == keyname:
                    if valname in value:
                        yield [], node
                for result in cls._find_kv_pair(value, keyname, valname):
                    path = node.get('_types', ['Unkwown'])[0]
                    result[0].append(path)
                    yield result
        elif isinstance(node, list):
            for pos, value in enumerate(node):
                for result in cls._find_kv_pair(value, keyname, valname):
                    path = f'[{pos}]'
                    result[0].append(path)
                    yield result

    def _parse_device(self, path: str, node: OMNode, names: List[str]) \
            -> Optional[Tuple[str, List[OMMemoryRegion], List[OMInterrupt],
                              Dict[str, OMDevice]]]:
        """Parse a single device.

           :param path: the path to the node
           :param node: the device root to parse
           :param names: accepted device names (if empty, accept all)
           :return: a 4-uple of device name, memory regions, interrupts and
                    one or more device/subdevice
        """
        types = [t.strip() for t in node.get('_types')]
        devtype = types[0]  # from most specialized to less specialized
        if not devtype.startswith('OM'):
            raise ValueError(f'Unexpected device types: {devtype}')
        name = devtype[2:].lower()
        if names and name not in names:
            return None
        devices = {}
        for region in node.get('memoryRegions', {}):
            if 'registerMap' not in region:
                continue
            regname = region['name'].lower()
            device = OMDevice(regname, self._regwidth)
            grpdescs, freggroups = self._parse_region(region)
            features = self._parse_features(node)
            freggroups = self._fuse_fields(freggroups)
            freggroups = self._scatgat_fields(freggroups)
            freggroups = self._factorize_fields(freggroups)
            device.descriptors = grpdescs
            device.fields = freggroups
            device.features = features
            devices[regname] = device
        mmaps = self._parse_memory_map(node)
        irqs = self._parse_interrupts(node)
        return name, mmaps, irqs, devices

    def _parse_region(self, region: OMNode) \
            -> Tuple[Dict[str, str],
                     Dict[str, Dict[str, List[OMRegField]]]]:
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
                regfield = OMRegField(HexInt(bitbase), bitsize, desc, reset,
                                      access)
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
            fodict = {name: group[name] for name in fnames}
            foffsets[grpname] = group[fnames[0]].offset
            registers[grpname] = fodict
        godict = {name: registers[name]
                  for name in sorted(registers, key=lambda n: foffsets[n])}
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
    def _parse_memory_map(cls, node: OMNode) -> List[OMMemoryRegion]:
        """Parse the memory map of a device.

           :param node: the object model compoent to parse
           :return: a list of memory regions
        """
        mmap = []
        for memregion in node.get('memoryRegions', []):
            for addrset in memregion.get('addressSets', []):
                name = memregion['name'].lower()
                mem = OMMemoryRegion(name,
                                     HexInt(addrset['base']),
                                     HexInt(addrset['mask']+1),
                                     memregion.get('description', ''))
            mmap.append(mem)
        return mmap

    @classmethod
    def _parse_interrupts(cls, node: OMNode) -> List[OMInterrupt]:
        """Parse interrupts definitions.

           :param node: the object model node to parse
           :return: a list of device interrupts
        """
        ints = []
        sections = []
        for section in node.get('interrupts', []):
            if not isinstance(section, dict) or '_types' not in section:
                continue
            types = section['_types']
            if 'OMInterrupt' not in types:
                continue
            sections.append(section)
        if not sections:
            return ints
        for pos, section in enumerate(sections):
            names = section['name'].lower().split('@', 1)
            name = re_sub(r'[\s\-]', '_', names[0])
            instance = HexInt(int(names[1], 16) if len(names) > 1 else 0)
            channel = section['numberAtReceiver']
            parent = section['receiver']
            ints.append(OMInterrupt(name, instance, channel, parent))
        return ints

    def _fuse_fields(self,
            reggroups: Dict[str, Dict[str, OMRegField]]) \
            -> Dict[str, Dict[str, OMRegField]]:
        """Merge 8-bit groups belonging to the same registers.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields
        """
        outregs = {}
        for gname, gregs in reggroups.items():  # HW group name
            mregs = {}
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
                                reg = OMRegField(HexInt(breg.offset), nsize,
                                                 breg.desc, nreset, breg.access)
                                nregs.append(reg)
                                breg = None
                        if breg is None:
                            breg = lreg
                            nsize = lreg.size
                            nreset = lreg.reset
                    if breg:
                        reg = OMRegField(HexInt(breg.offset), nsize, breg.desc,
                                         nreset, breg.access)
                        mregs[mname] = reg
            outregs[gname] = mregs
        return outregs

    def _scatgat_fields(self,
            reggroups: Dict[str, Dict[str, OMRegField]]) \
        -> Dict[str, Dict[str, OMRegField]]:
        """
        """
        outfields = DefaultDict(dict)
        rwidth = self._regwidth
        for gname, gregs in reggroups.items():  # HW group name
            first = None
            wmask = rwidth -1
            base = 0
            newfields = DefaultDict(list)
            fields = []
            skip = False
            for fname, field in gregs.items():
                while not skip:
                    if not first:
                        first = field
                        base = field.offset & ~wmask
                    fbase = field.offset-base
                    foffset = field.offset & wmask
                    if fbase < rwidth:
                        if fbase + field.size > rwidth:
                            # crossing a word boundary, skipping
                            skip = True
                            break
                        fields.append((fname, field))
                        break
                    newfields[gname].append(fields)
                    fields = []
                    first = None
            if skip:
                # cannot modify the fields, simply copy them over
                outfields[gname] = gregs
                continue
            if fields:
                if gname not in newfields:
                    newfields[gname] = []
                newfields[gname].append(fields)
            for name, groups in newfields.items():
                splitted = False
                for pos, fields in enumerate(groups):
                    if not splitted and len(fields) < 2:
                        fname, field = fields[0]
                        outfields[name][fname] = field
                        continue
                    newname = f'{name}_{pos}'
                    splitted = True
                    for fpair in fields:
                        fname, field = fpair
                        outfields[newname][fname] = field
        return outfields

    def _factorize_fields(self,
            reggroups: Dict[str, Dict[str, OMRegField]]) \
        ->  Dict[str, Tuple[Dict[str, OMRegField], int]]:
        """Search if identical fields can be factorized.
           This implementation is quite fragile and should be reworked,
           as it only works with simple cases.

           :param reggroups: register fields (indexed on group, name)
           :return: a dictionary of groups of register fields and repeat count
        """
        outregs = {}
        for gname, gregs in reggroups.items():
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
                field = OMRegField(HexInt(field0.offset), field0.size, cdesc,
                                   field0.reset, field0.access)
                greg = {}
                cname = cname.rstrip('_')
                greg[cname] = field
                outregs[gname] = (greg, repeat)
            else:
                outregs[gname] = (gregs, 1)
        return outregs

    @classmethod
    def _merge_memory_regions(cls,
        memregions: Sequence[Tuple[str, Sequence[OMMemoryRegion]]]) \
            -> Dict[str, OMMemoryRegion]:
        """Merge device memory map into a general platform map.

           This is a straightforware merge, which simply detects collision

           :param memregions: the device memory maps
           :return: the platform map
        """
        mmap = {}
        for name, devmap in memregions:
            iname = name.replace('-', '_')
            if len(devmap) == 1:
                mmap[iname] = devmap[0]
            else:
                for mreg in devmap:
                    subname = mreg.desc.replace(' ', '_').lower()
                    dname = f'{iname}_{subname}'
                    mmap[dname] = mreg
        ommap = {}
        # sort devices by address
        for name in sorted(mmap, key=lambda n: mmap[n].base):
            ommap[name] = mmap[name]
        return ommap

    @classmethod
    def _merge_interrupts(cls,
            interrupts: Sequence[Tuple[str, Sequence[OMInterrupt]]]) \
        -> Dict[str, Dict[str, int]]:
        """Merge interrup map into a general platform map.

           This is a straightforware merge, which simply detects collision

           :param irqs: the device interrupt maps
           :return: the platform interrupt map (parent, (name, irq))
        """
        imap = {}
        domain = {}
        for name, ints in interrupts:
            for interrupt in ints:
                # hack ahead:
                # the right hand side of '@' seems to be the destination if the
                # source is an interrupt controller, but seems to be the source
                # if the destination is an interrupt controller!
                # e.g. a device uses receiver=intc@device_address, while
                #      a intc use receiver=cpu@cpu_address (!?)
                # use the length to discriminate cases for now, but there's
                # something odd to address...
                pnames = interrupt.parent.split('@', 1)
                if len(pnames) == 1 or len(pnames[1]) < 4:
                    parent = interrupt.parent.replace('-', '_')
                else:
                    parent = pnames[0].replace('-', '_')
                if parent not in domain:
                    domain[parent] = set()
                if interrupt.channel in domain[parent]:
                    raise ValueError(f'Interrupt {interrupt.channel} redefined '
                                     f'in domain {parent}')
                domain[parent].add(interrupt.channel)
            if parent not in imap:
                imap[parent] = {}
            if name not in imap[parent]:
                imap[parent][name] = []
            imap[parent][name].extend(ints)
        for intdomain in imap.values():
            for ints in intdomain.values():
                ints.sort(key=lambda i: i.channel)
        oimap = {}
        for parent in sorted(imap):
            pimap = imap[parent]
            for device in sorted(pimap, key=lambda d: pimap[d][0].channel):
                if parent not in oimap:
                    oimap[parent] = {}
                if device not in oimap[parent]:
                    oimap[parent][device] ={}
                oimap[parent][device] = imap[parent][device]
        return oimap
