#!/usr/bin/env python3

"""JSON Object Model to C header file generator."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop

from argparse import ArgumentParser, FileType
from os import makedirs
from os.path import isdir, join as joinpath
from sys import exit as sysexit, modules, stdin, stdout, stderr
from traceback import print_exc
from omtools.generator import OMHeaderGenerator
from omtools.parser import OMParser


def main(args=None) -> None:
    """Main routine"""
    debug = False
    generators = OMHeaderGenerator.generators()
    outfmts = [x.lower() for x in generators]
    try:
        module = modules[__name__]
        argparser = ArgumentParser(description=module.__doc__)

        argparser.add_argument('comp', nargs='*',
                               help='Component(s) to extract from model')
        argparser.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        argparser.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        argparser.add_argument('-O', '--dir',
                               help='Output directory')
        argparser.add_argument('-l', '--list', action='store_true',
                               default=False,
                               help=f'List object model devices')
        argparser.add_argument('-f', '--format', choices=outfmts,
                               default=outfmts[-1],
                               help=f'Output format (default: {outfmts[-1]})')
        argparser.add_argument('-w', '--width', type=int,
                               choices=(8, 16, 32, 64), default=None,
                               help='Force register width (default: auto)')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args(args)
        debug = args.debug

        omp = OMParser(debug=debug)
        compnames = [c.lower() for c in args.comp]
        omp.parse(args.input, compnames)
        if args.list:
            print('Components:', file=args.output)
            for comp in omp:
                print(f' {comp.name}', file=args.output)
            sysexit(0)
        count = 0
        for name in compnames:
            omp.get(name)
            count += 1
        regwidth = args.width or omp.xlen
        generator = generators[args.format.title()]
        if len(compnames) == 1 or (not compnames and count == 1):
            comp = list(omp.get(compnames[0]))[0]
            generator(debug=debug).generate_device(args.output, comp, regwidth)
        elif args.dir:
            if not isdir(args.dir):
                makedirs(args.dir)
            if not compnames:
                compnames = [c.name for c in omp]
            for name in compnames:
                for comp in omp.get(name):
                    filename = joinpath(args.dir, f'sifive_{comp.name}.h')
                    with open(filename, 'wt') as ofp:
                        #print(f'Generating {name} as {filename}',
                        #      file=args.output)
                        generator(debug=debug).generate_device(ofp, comp,
                                                               regwidth)
            filename = joinpath(args.dir, f'sifive_platform.h')
            with open(filename, 'wt') as ofp:
                #print(f'Generating platform file as {filename}',
                #      file=args.output)
                generator().generate_platform(
                    ofp, omp.memory_map, omp.interrupt_map, omp.xlen)

    except (IOError, OSError, ValueError) as exc:
        print('Error: %s' % exc, file=stderr)
        if debug:
            print_exc(chain=False, file=stderr)
        sysexit(1)
    except KeyboardInterrupt:
        sysexit(2)


if __name__ == '__main__':
    from sys import argv
    if len(argv) > 1:
        main()
    else:
        # main('-i /Users/eblot/Downloads/s54_fpu_d-arty.objectModel.json -d plic'.split())
        main('-i /Users/eblot/Downloads/hcaDUT.objectModel.json -d hca'.split())
        # main('-i /Users/eblot/Downloads/e24_hca.objectModel.json -d hca'.split())
        # main('-i /Users/eblot/Downloads/s54_fpu_d-arty.objectModel.json -d UART'.split())