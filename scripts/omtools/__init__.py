"""Object Model Tools"""

from sys import version_info

if version_info[:2] < (3 ,7):
    # expect Dict to always be OrderedDict
    raise RuntimeError('Python 3.7+ is required')
