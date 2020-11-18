from importlib import import_module
from os import listdir
from os.path import dirname, isfile, join as joinpath, splitext
from typing import List
from ... import classproperty


class Devices:
    """
    """

    _DEVMODS = []

    @classproperty
    def modules(cls) -> List[str]:
        if not cls._DEVMODS:
            devpath = dirname(__file__)
            devmods = [f'{__name__}.{splitext(f)[0]}' for f in listdir(devpath)
                       if isfile(joinpath(devpath, f)) and
                       not f.startswith('_')]
            cls._DEVMODS = devmods
        return cls._DEVMODS
