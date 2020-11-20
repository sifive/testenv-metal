from functools import partial
from pprint import pprint as _pprint

pprint = partial(_pprint, sort_dicts=False)
