#!/usr/bin/env python3

"""JSON Object Model to C header file generator."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop


from argparse import ArgumentParser, FileType
from copy import copy
from enum import Enum
from json import loads as json_loads
from os.path import basename
from re import sub as re_sub
from sys import exit as sysexit, modules, stdin, stdout, stderr
from textwrap import dedent
from traceback import print_exc
from typing import (Any, DefaultDict, Dict, List, NamedTuple, OrderedDict,
                    Optional, TextIO, Tuple)
try:
    from jinja2 import Environment as JiEnv
except ImportError:
    JiEnv = None


from pprint import pformat, pprint

Access = Enum('Access', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class RegField(NamedTuple):
    """Register description."""
    offset: int
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class ObjectModelConverter:
    """JSON object model converter.

       :param names: one or more IP names to extract from the object model
    """

    HEADER = """
    /**
     * %s registers
     * @file %s
     *
     * @copyright (c) 2020 SiFive, Inc
     * @copyright SPDX-License-Identifier: MIT
     */
    """

    EXTRA_SEP_COUNT = 2

    def __init__(self, *names: List[str]):
        self._comps: \
            Dict[str,
                 Optional[
                     Tuple[Dict[str, str],
                           OrderedDict[str, OrderedDict[str, RegField]]],
                           Dict[str, Dict[str, bool]]
                 ]
            ] = {x.lower(): None for x in names}

    def parse(self, mfp: TextIO) -> None:
        """Parse an object model text stream.

           :param mfp: object model file-like object
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
            if 'memoryRegions' not in component:
                continue
            for region in component['memoryRegions']:
                if 'registerMap' not in region:
                    continue
                regname = region['name'].lower()
                if regname not in self._comps:
                    continue
                grpdescs, reggroups = self._parse_region(region)
                features = self._parse_features(component)
                mreggroups = self._merge_registers(reggroups)
                self._comps[regname] = (grpdescs, mreggroups, features)

    def generate_legacy_header(self, ofp: TextIO, name: str,
                               bitsize: int) -> None:
        """Generate a header file stream for a component.

           :param ofp: the output stream
           :param name: the name of a previously parsed component
           :param bitsize: the max width of register, in bits
        """
        try:
            grpdescs, grpfields, features = self._comps[name.lower()]
        except (KeyError, TypeError) as exc:
            raise ValueError(f'Unknown component name: {name}') from exc
        newgroups = self._split_registers(grpfields, bitsize)
        prefix = name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{name}.h'
        self._generate_legacy_prolog(ofp, filename, prefix)
        self._generate_legacy_features(ofp, prefix, features)
        self._generate_legacy_registers(ofp, prefix, newgroups, grpdescs)
        print('', file=ofp)
        self._generate_legacy_fields(ofp, bitsize, prefix, newgroups,
                                     grpdescs)
        self._generate_legacy_epilog(ofp, filename)

    @classmethod
    def _parse_region(cls, region: Dict[str, Any]) \
            -> Tuple[Dict[str, str],
                     OrderedDict[str, OrderedDict[str, List[RegField]]]]:
        """Parse an object model region containing a register map.

            :param region: the region to parse
            :return: a 2-tuple of a group dictionay which gives the description
                     string for each named group, and a dictionary of groups of
                     register fields
        """
        regmap = region['registerMap']
        groups = {}
        for group in regmap["groups"]:
            name = group['name']
            desc = group.get('description', '').strip()
            if desc.lower().endswith(' register'):
                desc = desc[:-len(' register')]
            if name not in groups:
                groups[name] = desc
            elif not groups[name]:
                groups[name] = desc
        registers = DefaultDict(dict)
        for field in  regmap['registerFields']:
            bitbase = field['bitRange']['base']
            bitsize = field['bitRange']['size']
            regfield = field['description']
            name = regfield['name']
            if name == 'reserved':
                continue
            desc = regfield['description']
            group = regfield['group']
            reset = regfield.get('resetValue', None)
            access_ = list(filter(lambda x: x in Access.__members__,
                                  regfield['access'].get('_types', '')))
            access = Access[access_[0]] if access_ else None
            regfield = RegField(bitbase, bitsize, desc, reset, access)
            registers[group][name] = regfield
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
    def _merge_registers(cls,
        reggroups: OrderedDict[str, OrderedDict[str, RegField]]) \
            -> OrderedDict[str, OrderedDict[str, RegField]]:
        """Merge 8-bit groups belonging to the same registers.

            :param reggroups: register fields (indexed on group, name)
            :return: a dictionary of groups of register fields
        """
        outregs = OrderedDict()
        for gname, gregs in reggroups.items():  # HW group name
            mregs = OrderedDict()
            # group registers of the same name radix into lists
            for fname in sorted(gregs, key=lambda n: gregs[n].offset):
                bfname = re_sub(r'_\d+', '', fname)  # register name w/o index
                if bfname not in mregs:
                    mregs[bfname] = []
                mregs[bfname].append(gregs[fname])
            # merge siblings into a single register
            for mname in mregs:
                lregs = mregs[mname]
                if len(lregs) == 1:
                    continue
                breg = None
                nsize = None
                nreset = None
                nregs = []
                for lreg in lregs:
                    if breg:
                        if lreg.offset == breg.offset+nsize:
                            if lreg.access != breg.access:
                                raise ValueError('Incoherent access modes')
                            nreset |= lreg.reset << nsize
                            nsize += lreg.size
                        else:
                            reg = RegField(breg.offset, nsize, breg.desc,
                                           nreset, breg.access)
                            nregs.append(reg)
                            breg = None
                    if breg is None:
                        breg = lreg
                        nsize = lreg.size
                        nreset = lreg.reset
                if breg:
                    reg = RegField(breg.offset, nsize, breg.desc,
                                   nreset, breg.access)
                    nregs.append(reg)
                mregs[mname] = nregs
            outregs[gname] = mregs
        return outregs

    @classmethod
    def _split_registers(cls,
        reggroups: OrderedDict[str, OrderedDict[str, RegField]],
        bitsize: int) -> OrderedDict[str, OrderedDict[str, RegField]]:
        """Split register fields into bitsized register words.

            :param reggroups: register fields (indexed on group, name)
            :param bitsize: max width of output registers
            :return: a dictionary of groups of register fields
        """
        outgroups = OrderedDict()
        for gname in reggroups:
            registers = reggroups[gname]
            fregs = OrderedDict()
            for regname in registers:
                fields = registers[regname]
                if len(fields) > 1:
                    # field with "holes"
                    raise NotImplementedError('Not handled')
                field = fields[0]
                if field.size <= bitsize:
                    fregs[regname] = field
                    continue
                mask = (1 << bitsize) - 1
                for idx in range(0, field.size, bitsize):
                    wsize = idx//bitsize
                    wreset = (field.reset >> idx) & mask
                    fregs[f'{regname}_{wsize}'] = RegField(
                        field.offset+idx, min(bitsize, field.size-idx),
                        f'{field.desc} {wsize}', wreset, field.access)
            outgroups[gname] = fregs
        return outgroups

    @classmethod
    def _generate_legacy_prolog(cls,
        out: TextIO, filename: str, name: str) -> None:
        """Generate file prolog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(dedent(cls.HEADER % (name, filename)).lstrip(), file=out)
        print(f'#ifndef {mult_ex}', file=out)
        print(f'#define {mult_ex}', file=out)
        print(file=out)

    @classmethod
    def _generate_legacy_epilog(cls, out: TextIO, filename: str) -> None:
        """Generate file epilog.

          :param out: the output text stream
        """
        mult_ex = f"{filename.upper().replace('.', '_').replace('-', '_')}_"
        print(file=out)
        print(f'#endif /* {mult_ex} */', file=out)

    @classmethod
    def _generate_legacy_features(cls,
        out: TextIO, prefix: str, features: Dict[str, Dict[str, bool]]) -> None:
        """Generate the definitions of the supported features.

           :param out: output stream
           :param prefix: the prefix for all register names
           :param features: a dict of supported features and options
        """
        if not features:
            return
        print(f'/* Supported {prefix} features */', file=out)
        print('', file=out)
        lengths = [len(x) for x in features]
        lengths.extend([len(x)+len(feat)+1
                        for feat in features.values() for x in feat])
        length = len(prefix) + len(prefix) + 1 + max(lengths)
        length += cls.EXTRA_SEP_COUNT
        for featname in features:
            feat_str = f'{prefix}_HAS_{featname.upper()}'
            print(f'#define {feat_str:{length}s} 1', file=out)
            subfeatures = features[featname]
            for subname in subfeatures:
                val = int(subfeatures[subname])
                sub_str = f'{prefix}_HAS_{featname.upper()}_{subname.upper()}'
                print(f'#define {sub_str:{length}s} {val}', file=out)
            print('', file=out)
        print('', file=out)

    @classmethod
    def _generate_legacy_registers(cls,
        out: TextIO, prefix: str,
        reggroups: OrderedDict[str, OrderedDict[str, List[RegField]]],
        groupdesc: Dict[str, str]) -> None:
        """Print out the register addresses (in bytes).

           :param out: output stream
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        length = (len(prefix) + len('REGISTER') +
                 max([len(x) for x in reggroups]) + 2 + cls.EXTRA_SEP_COUNT)
        for grpname, registers in reggroups.items():
            firstfield = list(registers.values())[0]
            # print('FIRST', firstfield)
            address = firstfield.offset//8
            addr_str = f'{prefix}_REGISTER_{grpname}'
            desc = groupdesc.get(grpname, '')
            print(f'/* {desc} */', file=out)
            print(f'#define {addr_str:{length}s} 0x{address:04X}u', file=out)

    @classmethod
    def _generate_legacy_fields(cls,
        out: TextIO, bitsize: int, prefix: str,
        reggroups: OrderedDict[str, OrderedDict[str, List[RegField]]],
        groupdesc: Dict[str, str]) -> None:
        """Print out the register content (offset and mask).

           :param out: output stream
           :param bitsize: the max register width in bits
           :param prefix: the prefix for all register names
           :param reggroups: the group of register fields to print out
           :param groupdesc: the description of each group
        """
        mask = (1 << bitsize) - 1
        length = max([len(grpname) +
                      max([len(regname) for regname in reggroups[grpname]])
                          for grpname in reggroups])
        length += len(prefix) + len('REGISTER') + len('OFFSET') + 4
        length += cls.EXTRA_SEP_COUNT
        for grpname, group in reggroups.items():
            desc = groupdesc.get(grpname, '')
            print(f'/*\n * {desc}\n */', file=out)
            base_offset = None
            for fname, field in group.items():
                if base_offset is None:
                    base_offset = field.offset
                    offset = 0
                else:
                    offset = field.offset - base_offset
                mask = (1 << field.size) - 1
                print(f'/* {field.desc} [{field.access.name}] */', file=out)
                off_str = f'{prefix}_REGISTER_{grpname}_{fname}_OFFSET'
                msk_str = f'{prefix}_REGISTER_{grpname}_{fname}_MASK'
                padding = max(2, 2*((field.size+7)//8))
                print(f'#define {off_str:{length}s} {offset}u', file=out)
                print(f'#define {msk_str:{length}s} 0x{mask:0{padding}X}u',
                      file=out)
            print('', file=out)

    def generate_si5sis_header(self, ofp: TextIO, name: str,
                               bitsize: int) -> None:
        """Generate a legacy header file stream for a component.

           :param out: the output stream
           :param name: the name of a previously parsed component
           :param bitsize: the max width of register, in bits
        """
        env = JiEnv(trim_blocks=False)
        # env = JiEnv(trim_blocks=False)
        # template = env.from_string(modules[__name__].SIS_TEMPLATE.lstrip())
        from os.path import join as joinpath, splitext
        jinja = joinpath('.'.join((splitext(__file__)[0], 'j2')))
        with open(jinja, 'rt') as jfp:
            template = env.from_string(jfp.read())
        try:
            grpdescs, grpfields, features = self._comps[name.lower()]
        except (KeyError, TypeError) as exc:
            raise ValueError(f'Unknown component name: {name}') from exc
        groups = self._split_registers(grpfields, bitsize)
        ucomp = name.upper()
        if ofp.name and not ofp.name.startswith('<'):
            filename = basename(ofp.name)
        else:
            filename = f'sifive_{name}.h'
        # pprint(groups)
        #grpsizes = {n: (sum([f.size for fields in grpfields[n].values()
        #                    for f in fields])+31)//32
        #            for n in grpnames}
        class hexint(int):
            def __repr__(self):
                return f'0x{self:04x}'

        grpsizes = OrderedDict()
        for name, group in groups.items():
            fields = list(group.values())
            size = fields[-1].offset + fields[-1].size - fields[0].offset
            # grpsizes[name] = hexint(fields[0].offset//8), (size+7)//8
            grpsizes[name] = hexint(fields[0].offset//8), size
        #pprint(grpsizes)
        #exit(0)

        cgroups = OrderedDict()
        last_pos = 0
        rsv = 0
        length = 40
        for name, group in groups.items():
            fields = list(group.values())
            if last_pos:
                padding = fields[0].offset-last_pos
                if padding > 0:
                    size = (padding+31)//32
                    rname = f'_RESERVED{rsv}'
                    padding = ' ' * (length - len(rname))
                    cgroups[rname] = \
                        (RegField('PADDING', ['all', size], '', 0, Access.R),
                         size, '', padding)
                    rsv += 1
            size = fields[-1].offset + fields[-1].size - fields[0].offset
            padding = ' ' * (length - len(name))
            cgroups[name] = (group, (size+31)//32, grpdescs[name], padding)
            last_pos = fields[-1].offset + fields[-1].size
        #pprint(cgroups)
        #exit(0)

        # shallow copy to avoid polluting locals dir
        text = template.render(copy(locals()))
        ofp.write(text)
        return

        length = (len(ucomp) + max([len(x) for x in grpdescs]) + 1 +
                  self.EXTRA_SEP_COUNT)
        for grpname in grpnames:
            group = reggroups[grpname]
            regname = sorted(group, key=lambda n: group[n].offset)[0]
            address = group[regname].offset//8
            addr_str = f'{ucomp}_REGISTER_{grpname}'
            desc = groupdesc.get(grpname, '')
            print(f'/* {desc} */', file=out)
            print(f'#define {addr_str:{length}s} 0x{address:04X}u', file=out)


SIS_TEMPLATE = """
/**
 * {{ ucomp }} registers
 * @file {{ filename }}
 *
 * @copyright (c) 2020 SiFive, Inc
 * @copyright SPDX-License-Identifier: MIT
 */

#ifndef SIFIVE_{{ ucomp }}_H_
#define SIFIVE_{{ ucomp }}_H_

typedef struct _{{ ucomp }} {
{% set last_pos = 0 %}
{% set wordsize = bitsize//32 %}
{% for name in grpnames %}
    {%if last_pos %}

    {% endif %}
    {% if grpsizes[name] == 1 %}
    uint32_t {{ name }}; /**< {{ grpsizes[name] }} */
    {% else %}
    uint32_t {{ name }}[{{ grpsizes[name] }}U]; /**< {{ grpsizes[name] }} */
    {% endif %}
{% endfor %}
} {{ ucomp }}_Type;

#endif /* SIFIVE_{{ ucomp }}_H_ */
"""



def main(args=None) -> None:
    """Main routine"""
    debug = False
    outfmts = ['legacy']
    if JiEnv is not None:
        outfmts.append('si5sis')
    try:
        argparser = ArgumentParser(description=modules[__name__].__doc__)

        argparser.add_argument('comp', nargs=1,
                               help='Component(s) to extract from model')
        argparser.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        argparser.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        argparser.add_argument('-f', '--format', choices=outfmts,
                               default=outfmts[-1],
                               help=f'Output format (default: {outfmts[-1]})')
        argparser.add_argument('-w', '--width', type=int,
                               choices=(8, 16, 32, 64), default=64,
                               help='Output register width (default: 64)')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args(args)
        debug = args.debug

        component = args.comp[0]
        omc = ObjectModelConverter(component)
        omc.parse(args.input)
        generator = getattr(omc, f'generate_{args.format}_header')
        generator(args.output, component,  args.width)

    except (IOError, OSError, ValueError) as exc:
        print('Error: %s' % exc, file=stderr)
        if debug:
            print_exc(chain=False, file=stderr)
        sysexit(1)
    except SystemExit as exc:
        if debug:
            print_exc(chain=True, file=stderr)
        raise
    except KeyboardInterrupt:
        sysexit(2)


if __name__ == '__main__':
    from sys import argv
    if len(argv) > 1:
        main()
    else:
        main('-i /Users/eblot/Downloads/hcaDUT.objectModel.json -w 64 hca'.split())
