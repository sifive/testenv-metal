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
                    Optional, TextIO, Tuple)
from .model import OMAccess, OMComponent, OMRegField


class OMParser:
    """JSON object model converter.

       :param regwidth: the defaut register width
       :param debug: set to report more info on errors
    """

    def __init__(self, regwidth: int = 32, debug=False):
        self._regwidth = regwidth
        self._debug = debug
        self._components: Dict[str, Optional[OMComponent]] = {}

    def get(self, name) -> OMComponent:
        """Return a componen from its object model name, if any.

           :return: the parsed component
        """
        try:
            return self._components[name]
        except KeyError as exc:
            raise ValueError(f"No '{name}' component in object model") from exc

    def __iter__(self) -> Iterator[OMComponent]:
        """Return an iterator for all parsed component, ordered by name.

           :return: a component iterator
        """
        for name in sorted(self._components):
            yield self._components[name]

    def parse(self, mfp: TextIO, names: Optional[List[str]] = None) -> None:
        """Parse an object model text stream.

           :param mfp: object model file-like object
           :param names: the component names to parse, or an empty list to
                         parse all components.
        """
        objmod = json_loads(mfp.read())
        if not isinstance(objmod, list):
            raise ValueError('Invalid format')
        objs = objmod[0]
        if not isinstance(objs, dict):
            raise ValueError('Invalid format')
        components = objs['components']
        if not isinstance(components, list):
            raise ValueError('Invalid format')
        for component in components:
            self._parse_component(component, names or [])

    def _parse_component(self, component: Dict[str, Any],
                         names: List[str]) -> None:
        """Parse a single compoment

           :param component: the component root to parse
           :param names: accepted component names (if empty, accept all)
        """
        for subcomp in component.get('components', []):
            self._parse_component(subcomp, names)
        for region in component.get('memoryRegions', {}):
            if 'registerMap' not in region:
                continue
            regname = region['name'].lower()
            if names and regname not in names:
                continue
            width = component.get('beatBytes', 32)
            comp = OMComponent(regname, width)
            grpdescs, reggroups = self._parse_region(region)
            features = self._parse_features(component)
            if width:
                features['data_bus'] = {'width': width}
            ints = self._parse_interrupts(component)
            freggroups = self._fuse_fields(reggroups)
            freggroups = self._factorize_fields(freggroups)
            comp.descriptors = grpdescs
            comp.fields = freggroups
            comp.features = features
            comp.interrupts = ints
            self._components[regname] = comp

    def _parse_region(self, region: Dict[str, Any]) \
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
    def _parse_features(cls, component: Dict[str, Any]) \
            -> Dict[str, Dict[str, bool]]:
        """Parse the supported feature of a component.cls

           :param component: the component to parse
           :return: a dictionary of supported features, with optional options
        """
        features = {}
        for sectname in component:
            section = component[sectname]
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
    def _parse_interrupts(cls, component: Dict[str, Any]) \
            -> OrderedDict[str, Tuple[str, int]]:
        """Parse interrupts definitions.

           :param component: the object model compoent to parse
           :return: an ordered map of (name, (controller, channel))
        """
        ints = {}
        for section in component.get('interrupts', []):
            if not isinstance(section, dict) or '_types' not in section:
                continue
            types = section['_types']
            if 'OMInterrupt' not in types:
                continue
            name = section['name'].replace('@', '_')
            channel = section['numberAtReceiver']
            parent = section['receiver']
            ints[name] = (parent, channel)
        interrupts = OrderedDict(
            ((i, ints[i]) for i in sorted(ints, key=lambda i: ints[i])))
        return interrupts

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
