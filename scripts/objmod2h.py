#!/usr/bin/env python3

"""JSON Object Model to C header file generator."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop

from argparse import ArgumentParser, FileType
from importlib import import_module
from logging import DEBUG, ERROR, Formatter, StreamHandler, getLogger
from os import listdir, makedirs
from os.path import basename, dirname, isdir, isfile, join as joinpath, splitext
from sys import exit as sysexit, modules, stdin, stdout, stderr
from traceback import print_exc
from typing import Dict, Type
from omtools.om.generator import OMHeaderGenerator
from omtools.parser import OMParser


def configure_logger(verbosity: int, debug: bool) -> None:
    """Configure logger format and verbosity.

       :param verbosity: the verbosity level
       :param debug: debug mode
    """
    loglevel = max(DEBUG, ERROR - (10 * (verbosity or 0)))
    loglevel = min(ERROR, loglevel)
    if debug:
        formatter = Formatter('%(asctime)s.%(msecs)03d %(levelname)-7s '
                              '%(name)-16s %(funcName)s[%(lineno)4d] '
                              '%(message)s', '%H:%M:%S')
    else:
        formatter = Formatter('%(message)s')
    log = getLogger('om')
    log.setLevel(loglevel)
    handler = StreamHandler(stderr)
    handler.setFormatter(formatter)
    log.addHandler(handler)


def main(args=None) -> None:
    """Main routine"""
    debug = False
    generators = OMHeaderGenerator.generators
    generators = {x.lower(): generators[x] for x in generators}
    default_gen = list(generators)[-1]
    try:
        module = modules[__name__]
        argparser = ArgumentParser(description=module.__doc__)

        argparser.add_argument('comp', nargs='*',
                               help='Component(s) to extract from model')
        argparser.add_argument('-l', '--list', action='store_true',
                               default=False,
                               help=f'List object model devices')

        files = argparser.add_argument_group(title='Files')
        files.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        files.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        files.add_argument('-O', '--dir',
                               help='Output directory')

        gen = argparser.add_argument_group(title='Generation')
        gen.add_argument('-f', '--format', choices=generators,
                         default=default_gen,
                         help=f'Output format (default: {default_gen})')
        gen.add_argument('-w', '--width', type=int,
                         choices=(8, 16, 32, 64), default=None,
                         help='Force register width (default: auto)')
        gen.add_argument('-t', '--test', action='store_true',
                         default=False,
                         help=f'Generate C test file to check files')

        extra = argparser.add_argument_group(title='Extras')
        extra.add_argument('-v', '--verbose', action='count', default=0,
                           help='Increase verbosity')
        extra.add_argument('-d', '--debug', action='store_true',
                           help='Enable debug mode')

        args = argparser.parse_args(args)
        debug = args.debug
        configure_logger(args.verbose, debug)
        log = getLogger('om.main')

        omp = OMParser(debug=debug)
        compnames = [c.lower() for c in args.comp]
        omp.parse(args.input, compnames)
        if args.list:
            log('Components')
            for comp in omp.device_iterator:
                log.info('%s', comp.name)
            sysexit(0)
        count = 0
        for name in compnames:
            omp.get_devices(name)
            count += 1
        regwidth = args.width or omp.xlen
        generator = generators[args.format]
        if len(compnames) == 1 or (not compnames and count == 1):
            comp = list(omp.get_devices(compnames[0]))[0]
            generator(debug=debug).generate_device(args.output, comp, regwidth)
        elif args.dir:
            header_files = []
            if not isdir(args.dir):
                makedirs(args.dir)
            if not compnames:
                compnames = {c.name for c in omp.device_iterator}
            for name in compnames:
                gen = generator.get_generator(name)(test=args.test, debug=debug)
                for comp in omp.get_devices(name):
                    filename = joinpath(args.dir, f'sifive_{comp.name}.h')
                    with open(filename, 'wt') as ofp:
                        log.info('Generating %s as %s', name, filename)
                        try:
                            gen.generate_device(ofp, comp, regwidth)
                        except Exception:
                            log.error('Cannot generate header for device %s '
                                      'with generator %s',
                                      name, gen.__class__.__name__)
                            raise
                    header_files.append(basename(filename))
            if compnames:
                filename = joinpath(args.dir, f'sifive_defs.h')
                with open(filename, 'wt') as ofp:
                    if args.verbose:
                        log.info('Generating definition file as %s', filename)
                    gen.generate_definitions(ofp)
                    header_files.append(basename(filename))
            filename = joinpath(args.dir, f'sifive_platform.h')
            with open(filename, 'wt') as ofp:
                log.info('Generating platform file as %s', filename)
                gen.generate_platform(ofp, omp.memory_map, omp.interrupt_map,
                                      omp.xlen)
                header_files.append(basename(filename))
            if args.test:
                filename = joinpath(args.dir, f'sifive_selftest.c')
                with open(filename, 'wt') as ofp:
                    gen.generate_autotest(ofp, header_files)

    except (IOError, OSError, ValueError) as exc:
        print('Error: %s' % exc, file=stderr)
        if debug:
            print_exc(chain=False, file=stderr)
        sysexit(1)
    except KeyboardInterrupt:
        sysexit(2)


if __name__ == '__main__':
    main()
