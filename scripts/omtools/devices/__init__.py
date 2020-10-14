from os.path import commonprefix
from pprint import pprint
from re import match as re_match, sub as re_sub
from sys import stderr
from typing import Dict, List, Tuple
from ..model import (HexInt, OMAccess, OMDeviceMap, OMInterrupt, OMMemoryRegion,
                     OMNode, OMRegField)


class OMDeviceParser:

    def __init__(self, regwidth: int, debug: bool = False):
        self._regwidth = regwidth
        self._debug = debug

    def parse(self, node: OMNode) \
            -> Tuple[Dict[str, OMDeviceMap],
                     List[OMMemoryRegion],
                     List[OMInterrupt]]:
        devices = {}
        for region in node.get('memoryRegions', {}):
            if 'registerMap' not in region:
                continue
            regname = region['name'].lower()
            device = OMDeviceMap(regname)
            grpdescs, features, freggroups = self._build_device(node, region)
            device.descriptors = grpdescs
            device.fields = freggroups
            device.features = features
            devices[regname] = device
        mmaps = self._parse_memory_map(node)
        irqs = self._parse_interrupts(node)
        return devices, mmaps, irqs

    def _build_device(self, node: OMNode, region: Dict) \
            -> Tuple[Dict[str, str],
                     Dict[str, Dict[str, bool]],
                     Dict[str, Tuple[Dict[str, OMRegField], int]]]:
        grpdescs, freggroups = self._parse_region(region)
        features = self._parse_features(node)
        freggroups = self._fuse_fields(freggroups)
        debug = self._debug
        from deepdiff import DeepDiff
        old = freggroups
        self._debug = region['name'].lower() == 'plic'
        freggroups = self._scatgat_fields(freggroups)
        if self._debug:
            pprint(DeepDiff(old, freggroups))
        freggroups = self._factorize_fields(freggroups)
        self._debug = debug
        return grpdescs, features, freggroups

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
        registers = {}
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
                if group not in registers:
                    registers[group] = {}
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
        namecount = {}
        for section in node.get('interrupts', []):
            if not isinstance(section, dict) or '_types' not in section:
                continue
            types = section['_types']
            if 'OMInterrupt' not in types:
                continue
            sections.append(section)
            name = section['name'].split('@')[0]
            if name not in namecount:
                namecount[name] = 1
            else:
                namecount[name] += 1
        if not sections:
            return ints
        for pos, section in enumerate(sections):
            names = section['name'].lower().split('@', 1)
            name = re_sub(r'[\s\-]', '_', names[0])
            instance = HexInt(int(names[1], 16) if len(names) > 1 else 0)
            channel = section['numberAtReceiver']
            parent = section['receiver']
            if namecount[names[0]] > 1:
                name = f'{name}{pos}'
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
                                raise NotImplementedError('Unsupported')
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

    def _scatgat_fields(self, reggroups: Dict[str, Dict[str, OMRegField]]) \
            -> Dict[str, Dict[str, OMRegField]]:
        """Gather and scatter bitfield in register words.

           Bitfields cannot be longer that a register word. Split them into
           consecutive register words whenever necessary.
        """
        outfields = {}
        rwidth = self._regwidth
        for gname, gregs in reggroups.items():
            first = None
            wmask = rwidth -1
            base = 0
            newfields = {}
            fields = []
            skip = False
            for fname, field in gregs.items():
                while not skip:
                    if not first:
                        first = field
                        base = field.offset & ~wmask
                    fbase = field.offset-base
                    if fbase < rwidth:
                        if fbase + field.size > rwidth:
                            # crossing a word boundary, skipping
                            skip = True
                            break
                        fields.append((fname, field))
                        break
                    if gname not in newfields:
                        newfields[gname] = []
                    newfields[gname].append(fields)
                    fields = []
                    first = None
            # cannot modify the fields
            if skip:
                # simply copy them over
                outfields[gname] = gregs
                # nothing more to do for this group, preserve the insertion
                # order in the output dictionary
                continue
            # the above loop may have left last fields not yet stored into
            # the new dictionary
            if fields:
                if gname not in newfields:
                    newfields[gname] = []
                newfields[gname].append(fields)
            for name, groups in newfields.items():
                splitted = False
                for pos, fields in enumerate(groups):
                    if not splitted and len(fields) < 2:
                        fname, field = fields[0]
                        if name not in outfields:
                            outfields[name] = {}
                        outfields[name][fname] = field
                        continue
                    # rename field group with an index suffix if there is
                    # more than one sub group
                    newname = f'{name}_{pos}' if len(groups) > 1 else name
                    if newname not in outfields:
                        outfields[newname] = {}
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
