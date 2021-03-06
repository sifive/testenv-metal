#!/usr/bin/env python3

"""Quick an d dirty JSON tree dumping tool"""

from argparse import ArgumentParser, FileType
from json import loads as json_loads
from os import getenv
from pprint import pprint
from sys import exit as sysexit, modules, stdin, stdout, stderr
from typing import Any, List, TextIO


class JsonTree:
    """JSON tree container."""

    def __init__(self):
        self._root = {}

    def load(self, ifp: TextIO) -> None:
        self._root = json_loads(ifp.read())

    def show_subtree(self, out: TextIO, depth: int) -> None:
        width = int(getenv('COLUMNS', '80'))
        pprint(self._root, width=width,depth=depth, stream=out, )

    def show_path(self, out: TextIO, item: str, icase: bool = False,
                  showval: bool = False) -> None:
        for result in self._find(item, self._root, icase, showval):
            path = []
            for node in reversed(result):
                if showval and node is None:
                    path.append(node)
                    continue
                if isinstance(node, int):
                    path.append(f'[{node}]')
                else:
                    if showval and path[-1] is None:
                        path[-1] = ' = '
                        path.append(str(node))
                    else:
                        path.append(f'.{node}')
            print(''.join(path), file=out)

    @classmethod
    def _find(cls, item: str, node: Any, icase: bool, showval: bool):
        if isinstance(node, dict):
            for key, value in node.items():
                for result in cls._find(item, key, icase, showval):
                    if showval and isinstance(value, (int, str, bool)):
                        result.append(None)
                        if isinstance(value, int):
                            result.append(f'{value} / 0x{value:x}')
                        else:
                            result.append(value)
                        yield list(reversed(result))
                    elif isinstance(value, list) and \
                         all([isinstance(v, (int, str, bool)) for v in value]):
                        result.append(None)
                        result.append(', '.join([str(v) for v in value]))
                        yield list(reversed(result))
                    else:
                        yield result
                for result in cls._find(item, value, icase, showval):
                    result.append(str(key))
                    yield result
        elif isinstance(node, list):
            for pos, value in enumerate(node):
                for result in cls._find(item, value, icase, showval):
                    result.append(pos)
                    yield result
        elif item == node:
            yield [str(node)]
        elif icase and isinstance(node, str):
            if item == node.lower():
                yield [str(node)]


def main(args=None) -> None:
    """Main routine"""
    debug = False
    try:
        module = modules[__name__]
        argparser = ArgumentParser(description=module.__doc__)

        argparser.add_argument('-i', '--input', type=FileType('rt'),
                               default=stdin,
                               help='Input header file')
        argparser.add_argument('-o', '--output', type=FileType('wt'),
                               default=stdout,
                               help='Output header file')
        argparser.add_argument('-c', '--cut', type=int,
                               help='Tree depth cut')
        argparser.add_argument('-p', '--path',
                               help='Show path to element')
        argparser.add_argument('-t', '--type', choices=('str', 'int', 'bool'),
                               default='str',
                               help='Element type')
        argparser.add_argument('-I', '--case-insensitive', action='store_true',
                               default=False,
                               help='Case-insensitive match')
        argparser.add_argument('-V', '--value', action='store_true',
                               default=False,
                               help='Show value')
        argparser.add_argument('-d', '--debug', action='store_true',
                               help='Enable debug mode')

        args = argparser.parse_args(args)
        debug = args.debug
        jst = JsonTree()
        jst.load(args.input)
        if args.cut is not None:
            jst.show_subtree(args.output, args.cut)
        if args.path is not None:
            element = args.path
            try:
                if args.type == 'int':
                    element = int(element)
                elif args.type == 'bool':
                    element = bool(element)
            except ValueError as exc:
                raise ValueError(f"Cannot convert '{args.path}' as '"
                                 f"{args.type}'")
            if args.case_insensitive:
                element = element.lower()
            jst.show_path(args.output, element, args.case_insensitive,
                          args.value)

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
    main()
