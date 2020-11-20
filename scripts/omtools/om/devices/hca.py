from typing import Dict, Union
from ..generator import OMSifiveSisHeaderGenerator
from .. import pprint


class OMSifiveSisHcaHeaderGenerator(OMSifiveSisHeaderGenerator):
    """SiFive SIS header generator for HCA crypto engine.
    """

    def _generate_definitions(self, devname: str,
            features: Dict[str, Dict[str, Union[bool, int]]]) -> Dict[str, int]:
        """Generate constant definitions.

           :param devname: the device name
           :param features:  map of supported features and subfeatures.
           :return: a map of definition name, values
        """
        # implementation is device specific
        defs = {}
        uname = devname.upper()
        for feature in features:
            ufeat = feature.upper()
            defs[f'{uname}_HAS_{ufeat}'] = 1
        for feature, subfeats in features.items():
            for subfeat, value in subfeats.items():
                ufeat = feature.upper()
                usfeat = subfeat.upper()
                defs[f'{uname}_{ufeat}_HAS_{usfeat}'] = int(value or 0)
        maxwidth = max([len(c) for c in defs]) + self.EXTRA_SEP_COUNT
        pdefs = {}
        for name, value in defs.items():
            padlen = maxwidth - len(name)
            if padlen > 0:
                name = f"{name}{' '*padlen}"
            pdefs[name] = value
        return pdefs
