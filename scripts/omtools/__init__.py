"""Object Model Tools"""

from sys import version_info
from typing import Any

if version_info[:2] < (3 ,7):
    # expect Dict to always be OrderedDict
    raise RuntimeError('Python 3.7+ is required')

#pylint: disable-msg=invalid-name
class classproperty(property):
    """Getter property decorator for a class"""
    def __get__(self, obj: Any, objtype=None) -> Any:
        return super(classproperty, self).__get__(objtype)
#pylint: enable-msg=invalid-name
