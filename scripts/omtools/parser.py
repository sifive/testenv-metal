"""JSON Object Model parser."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=too-many-branches
#pylint: disable-msg=too-many-statements
#pylint: disable-msg=cell-var-from-loop

from importlib import import_module
from json import loads as json_loads
from os.path import commonprefix
from pprint import pprint
from re import match as re_match, sub as re_sub
from sys import modules, stderr
from typing import (Any, DefaultDict, Dict, Iterable, Iterator, List,
                    Optional, Sequence, Set, TextIO, Tuple, Type)
from .om.devices import Devices
from .om.model import (HexInt, OMAccess, OMCore, OMDeviceMap, OMInterrupt,
                       OMMemoryRegion, OMNode, OMPath, OMRegField)
from .om.parser import OMDeviceParser


class DiscardedItemError(ValueError):
    """Special error to discard a non-fatal but unsupported object model item.
    """


class OMParser:
    """JSON object model converter.

       :param regwidth: the defaut register width
       :param debug: set to report more info on errors
    """

    _DEVICE_PARSERS: Dict[str, OMDeviceParser] = {}

    def __init__(self, regwidth: int = 32, debug: bool = False):
        self._regwidth: int = regwidth
        self._xlen: Optional[int] = None
        self._debug: bool = debug
        self._cores: Dict[int, OMCore] = {}
        self._devices: Dict[str, Optional[OMDeviceMap]] = {}
        self._memorymap: Dict[str, OMMemoryRegion] = {}
        self._intmap: Dict[str, Dict[str, int]] = {}

    @classmethod
    def get_parser(cls, name: str) -> OMDeviceParser:
        """Return the parser for the specified device.

           :param name: the device to parse
           :return: the device parser
        """
        if not cls._DEVICE_PARSERS:
            for modname in Devices.modules:
                devmod = import_module(modname)
                for name in dir(devmod):
                    item = getattr(devmod, name)
                    if not isinstance(item, Type):
                        continue
                    if not issubclass(item, OMDeviceParser):
                        continue
                    sname = name.replace('OM', '').replace('DeviceParser', '')
                    cls._DEVICE_PARSERS[sname.lower()] = item
            # default parser
            cls._DEVICE_PARSERS[''] = OMDeviceParser
        name = name.lower()
        try:
            # device specific parser
            return cls._DEVICE_PARSERS[name.lower()]
        except KeyError:
            # default parser
            return cls._DEVICE_PARSERS['']

    def get_core(self, hartid: int) -> OMCore:
        """Return a core from its hart identifier, if any.

           :return: the parsed core
        """
        return self._cores[hartid]

    def get_devices(self, name: str) -> Iterable[OMDeviceMap]:
        """Return devices from their object model name, if any.

           :return: an iterator of all devices of the selected name
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

    @property
    def core_iterator(self) -> Iterator[int]:
        """Return an iterator on parsed hard ids, ordered by id.

           :return: a hart id iterator
        """
        for hartid in sorted(self._cores):
            yield hartid

    @property
    def device_iterator(self) -> Iterator[OMDeviceMap]:
        """Return an iterator on parsed device names, ordered by name.

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
        for path in self._find_node_of_type(objmod, 'OMCore'):
            core, hartid = self._parse_core(path)
            self._cores[hartid] = core
        xlens = set()
        for core in self._cores.values():
            xlens.add(core.xlen)
        if len(xlens) != 1:
            raise ValueError(f'Incoherent xlen in platfornm: {xlens}')
        self._xlen = xlens.pop()
        for path in self._find_node_of_type(objmod, 'OMDevice'):
            if path.embedded:
                # for now, ignore sub devices
                continue
            try:
                dev = self._parse_device(path, names or [])
            except DiscardedItemError as exc:
                print(exc, file=stderr)
                continue
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
    def _find_node_of_type(cls, root: Any, typename: str) \
            -> Iterator[OMPath]:
        for path in cls._find_kv_pair(root, '_types', typename):
            yield path

    @classmethod
    def _find_kv_pair(cls, node: Any, keyname: str, valname: str) \
            -> Iterator[OMPath]:
        if isinstance(node, dict):
            for key, value in node.items():
                if key == keyname:
                    if valname in value:
                        path = OMPath(node)
                        yield path
                for result in cls._find_kv_pair(value, keyname, valname):
                    result.add_parent(node)
                    yield result
        elif isinstance(node, list):
            for value in node:
                for result in cls._find_kv_pair(value, keyname, valname):
                    result.add_parent(node)
                    yield result

    def _parse_core(self, path: OMPath) -> Tuple[OMCore, int]:
        """Parse a single core.

           :param path: the path to the node
        """
        node = path.node
        hartids = node['hartIds']
        if len(hartids) != 1:
            raise ValueError(f'HartIds not handled {hartids}')
        isa = node['isa']
        exts = []
        xlens = set()
        iset = None
        for kname, value in isa.items():
            if kname == 'baseSpecification':
                continue
            if kname == 'xLen':
                xlens.add(value)
            if isinstance(value, dict):
                types =  value.get('_types', None)
                if not type:
                    continue
                if 'OMSpecification' in types:
                    exts.extend(kname)
                elif 'OMBaseInstructionSet' in types:
                    iset = types[0]
                    imo = re_match(r'RV(?P<xlen>\d+)(?P<ext>\w+)', iset)
                    if not imo:
                        raise ValueError(f'Unsupported type: {iset}')
                    xlens.add(int(imo.group('xlen')))
                    exts.extend(imo.group('ext').lower())
        if len(xlens) != 1:
            raise ValueError(f'xLen issue detected: {xlens}')
        core = OMCore(xlens.pop(), ''.join(exts))
        return core, hartids[0]

    def _parse_device(self, path: OMPath, names: List[str]) \
            -> Optional[Tuple[str, List[OMMemoryRegion], List[OMInterrupt],
                              Dict[str, OMDeviceMap]]]:
        """Parse a single device.

           :param path: the path to the node
           :param names: accepted device names (if empty, accept all)
           :return: a 4-uple of device name, memory regions, interrupts and
                    one or more device maps
        """
        node = path.node
        types = [t.strip() for t in node.get('_types')]
        devtype = types[0]  # from most specialized to less specialized
        if not devtype.startswith('OM'):
            raise DiscardedItemError(f'Unexpected device types: {devtype}')
        name = devtype[2:].lower()
        if names and name not in names:
            return None
        devparser = OMParser.get_parser(name)(self._regwidth, self._debug)
        devmaps, mmaps, irqs = devparser.parse(node)
        return name, mmaps, irqs, devmaps

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
