from difflib import SequenceMatcher
from typing import List


def common_desc(desc1: str, desc2: str) -> str:
    """Find the common description from two description strings.
       :param desc1: first description string
       :param desc2: second description string
       :return: common description
    """
    matcher = SequenceMatcher(None, desc1, desc2)
    match = matcher.find_longest_match(0, len(desc1), 0, len(desc2))
    desc = desc1[match.a:match.a+match.size].strip()
    return desc


def flatten(lst: List) -> List:
    """Flatten a list.
    """
    return [item for sublist in lst for item in sublist]
