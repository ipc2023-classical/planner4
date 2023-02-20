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
    r"Landmark graph contains (\d+) landmarks, of which \d+ are disjunctive and \d+ are conjunctive.",
    type=int)
parser.add_pattern(
    "landmarks_disjunctive",
    r"Landmark graph contains \d+ landmarks, of which (\d+) are disjunctive and \d+ are conjunctive.",
    type=int)
parser.add_pattern(
    "landmarks_conjunctive",
    r"Landmark graph contains \d+ landmarks, of which \d+ are disjunctive and (\d+) are conjunctive.",
    type=int)
parser.add_pattern(
    "orderings",
    r"Landmark graph contains (\d+) orderings.",
    type=int)

parser.add_pattern(
    "orderings_necessary",
    "Landmark graph has (\d+) necessary orderings.",
    type=int)
parser.add_pattern(
    "orderings_greedy-necessary",
    "Landmark graph has (\d+) greedy-necessary orderings.",
    type=int)
parser.add_pattern(
    "orderings_natural",
    "Landmark graph has (\d+) natural orderings.",
    type=int)
parser.add_pattern(
    "orderings_reasonable",
    "Landmark graph has (\d+) reasonable orderings.",
    type=int)
parser.add_pattern(
    "orderings_obedient-reasonable",
    "Landmark graph has (\d+) obedient-reasonable orderings.",
    type=int)

parser.add_pattern(
    "prog_goal",
    "Landmark progression marked landmarks future due to (\d+) goals, "
    "\d+ greedy-necessary orderings, and \d+ reasonable orderings.",
    type=int)
parser.add_pattern(
    "prog_gn",
    "Landmark progression marked landmarks future due to \d+ goals, "
    "(\d+) greedy-necessary orderings, and \d+ reasonable orderings.",
    type=int)
parser.add_pattern(
    "prog_r",
    "Landmark progression marked landmarks future due to \d+ goals, "
    "\d+ greedy-necessary orderings, and (\d+) reasonable orderings.",
    type=int)

parser.parse()
