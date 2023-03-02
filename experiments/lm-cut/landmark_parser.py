#! /usr/bin/env python

import re

from lab.parser import Parser

parser = Parser()
parser.add_pattern(
    "lmgraph_generation_time",
    r"Landmark graph generation time: (.+)s",
    type=float)
parser.add_pattern(
    "landmarks",
    r"Landmark graph contains (\d+) landmarks.",
    type=int)
parser.add_pattern(
    "orderings",
    r"Landmark graph contains (\d+) orderings.",
    type=int)

parser.parse()
