"""Object Model components."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop

from enum import Enum
from typing import (Dict, NamedTuple, OrderedDict, Optional, Tuple, Union)


OMAccess = Enum('OMAccess', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class OMRegField(NamedTuple):
    """Register description.
    """
    offset: int
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]



class OMComponent:
    """Object model component container

       :param name: the component name, as defined in the object model
    """

    def __init__(self, name: str, width: int = 0):
        self._name = name
        self._width = width
        self._descriptors: Dict[str, str] = {}
        self._fields: OrderedDict[str,
                                  Tuple[OrderedDict[str, OMRegField], int]] = {}
        self._features: Dict[str, Dict[str, Union[int, bool]]] = {}
        self._interrupts: OrderedDict[str, Tuple[str, int]] = {}

    @property
    def name(self) -> str:
        """Return the name of the component.

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

    @property
    def interrupts(self) -> OrderedDict[str, Tuple[str, int]]:
        """Return an ordered map of interrupt channels.

           Order is the channel number (from lowest to highest)

           :return: a map of (name, (controller, channel)).
        """
        return self._interrupts

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

    @interrupts.setter
    def interrupts(self, interrupts: Dict[str, str]) -> None:
        if not isinstance(interrupts, OrderedDict):
            raise ValueError('Invalid interrupts')
        self._interrupts = interrupts
