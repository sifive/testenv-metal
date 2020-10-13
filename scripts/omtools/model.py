"""Object Model elements."""

#pylint: disable-msg=too-many-arguments
#pylint: disable-msg=too-many-locals
#pylint: disable-msg=too-many-nested-blocks
#pylint: disable-msg=cell-var-from-loop

from enum import Enum
from typing import Any, Dict, List, NamedTuple, Optional, Tuple, Union


OMNode = Dict[str, Union['OMNode', List, str, int, bool]]
"""An object model node."""


OMAccess = Enum('OMAccess', 'R RW W RFWT_ONE_TO_CLEAR')
"""Register access types."""


class HexInt(int):
    """Integral number wrapper to help debugging."""

    def __repr__(self):
        """Represent the integer as an hexadecimal number."""
        return f'0x{self:x}'


class OMPath:
    """Storage to locate a node within an object model.

       :param node: the tracked node
    """

    def __init__(self, node: Any):
        self._leaf = node
        self._heir = []  # root is on right end, leaf on left end

    @property
    def node(self) -> OMNode:
        """Get the node.

           :return: the stored node.
        """
        return self._leaf

    @property
    def omtype(self) -> str:
        """Get the object model type of the node.

           :return: the type as a string.
        """
        try:
            types = self._leaf.get('_types')
        except (AttributeError, KeyError):
            return ''
        omtypes = self.get_om_types()
        return omtypes[0] if omtypes else ''

    @property
    def parent(self) -> 'OMPath':
        """Get the path to the parent node.

           :return: the path to the immediate parent.
        """
        parent = OMPath(self._heir[0])
        for hnode in self._heir[1:]:
            parent.add_parent(hnode)
        return parent

    @property
    def device(self) -> bool:
        """Tell if the node is an object model device.

           :return: True of the node is an object model device.
        """
        return 'OMDevice' in self.get_om_types()

    @property
    def embedded(self) -> bool:
        """Tell if the node is a sub-object of an object model node.

           :return: True of the node is sub object.
        """
        return self.parent.omtype != ''

    def is_of_kind(self, kind: str) -> bool:
        """Tell if the node is a specific kind.

           :return: True of the node is of this kind.
        """
        return kind in self.get_om_types()

    def add_parent(self, node: Any):
        """Add the parent to the current node hierarchy.

           :param node: add a new parent
        """
        self._heir.append(node)

    def get_om_types(self) -> List[str]:
        """Return the list of object model types, if any.

           :return: the list of OM types
        """
        try:
            types = self._leaf.get('_types')
        except (AttributeError, KeyError):
            return []
        return [t for t in types if t.startswith('OM')]

    def __str__(self):
        return repr(self)

    def __repr__(self):
        obj = self._leaf
        paths = []
        for pos, node in enumerate(self._heir):
            paths.append(self._make_path(obj, node))
            obj = node
        return '.'.join(reversed(paths))

    def _make_path(self, obj, node):
        if isinstance(node, list):
            for pos, item in enumerate(node):
                if item == obj:
                    return f'{[pos]}'
        if isinstance(node, dict):
            for name in node:
                if node[name] == obj:
                    return name
        raise ValueError(f'Node {type(node)} not found in parent')


class OMRegField(NamedTuple):
    """Register description.
    """
    offset: HexInt
    size: int
    desc: str
    reset: Optional[int]
    access: Optional[str]


class OMCore(NamedTuple):
    """Core description.
    """
    xlen: int
    isa: str


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

    def __init__(self, name: str):
        self._name = name
        self._descriptors: Dict[str, str] = {}
        self._fields: Dict[str,
                                  Tuple[Dict[str, OMRegField], int]] = {}
        self._features: Dict[str, Dict[str, Union[int, bool]]] = {}

    @property
    def name(self) -> str:
        """Return the name of the device.

           :return: the name
        """
        return self._name

    @property
    def descriptors(self) -> Dict[str, str]:
        """Return a map of human readable strings for each defined register
           group.

           :return: a map of descriptor (register name, description)
        """
        return self._descriptors

    @property
    def fields(self) -> Dict[str, Dict[str, OMRegField]]:
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
        if not isinstance(fields, Dict):
            raise ValueError('Invalid fields')
        self._fields = fields

    @features.setter
    def features(self, features: Dict[str, str]) -> None:
        if not isinstance(features, dict):
            raise ValueError('Invalid features')
        self._features = features
