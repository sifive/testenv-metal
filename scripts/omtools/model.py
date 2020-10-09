"""Object Model elements."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop

from enum import Enum
from typing import (Dict, List, NamedTuple, OrderedDict, Optional, Tuple, Union)


OMNode = Dict[str, Union['OMNode', List, str, int, bool]]
"""An object model node"""


OMAccess = Enum('OMAccess', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class HexInt(int):
    """Integral number wrapper to help debugging."""

    def __repr__(self):
        """Represent the integer as an hexadecimal number."""
        return f'0x{self:x}'


class OMRegField(NamedTuple):
    """Register description.
    """
    offset: HexInt
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class OMMemoryRegion(NamedTuple):
    """Device memory region."""

    name: str
    base: HexInt
    size: int
    desc: str


class OMInterrupt(NamedTuple):
    """Interrupt."""

    name: str
    instance: HexInt
    channel: int
    parent: str


class OMDevice:
    """Object model device container

       :param name: the device name, as defined in the object model
       :param width: the maximal register width in bits
    """

    def __init__(self, name: str, width: int = 0):
        self._name = name
        self._width = width
        self._descriptors: Dict[str, str] = {}
        self._fields: OrderedDict[str,
                                  Tuple[OrderedDict[str, OMRegField], int]] = {}
        self._features: Dict[str, Dict[str, Union[int, bool]]] = {}

    @property
    def name(self) -> str:
        """Return the name of the device.

           :return: the name
        """
        return self._name

    @property
    def width(self) -> str:
        """Return the data bus width.

           :return: the width in bytes
        """
        if not self._width:
            raise RuntimeError('Data bus width not known')
        return self._width

    @property
    def descriptors(self) -> Dict[str, str]:
        """Return a map of human readable strings for each defined register
           group.

           :return: a map of descriptor (register name, description)
        """
        return self._descriptors

    @property
    def fields(self) -> OrderedDict[str, OrderedDict[str, OMRegField]]:
        """Return an ordered map of registers.
           Each register is an ordered map of register fields.

           Order is the memory offset (from lowest to highest offsets)

           :return: a map of register field maps.
        """
        return self._fields

    @property
    def features(self) -> Dict[str, Dict[str, Union[bool, int]]]:
        """Return a map of supported features and subfeatures.

           :return: a map of subfeature maps.
        """
        return self._features

    @descriptors.setter
    def descriptors(self, descs: Dict[str, str]) -> None:
        if not isinstance(descs, dict):
            raise ValueError('Invalid descriptors')
        self._descriptors = descs

    @fields.setter
    def fields(self, fields: Dict[str, str]) -> None:
        if not isinstance(fields, OrderedDict):
            raise ValueError('Invalid fields')
        self._fields = fields

    @features.setter
    def features(self, features: Dict[str, str]) -> None:
        if not isinstance(features, dict):
            raise ValueError('Invalid features')
        self._features = features
