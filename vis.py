#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import tempfile
from graph_tool.all import *
from wrapper import Topologies

if (len(sys.argv) != 2) and (len(sys.argv) != 3):
    sys.exit("usage: vis.py <graph.dot> [ sfdp | fr | arf | plan | rand ]");

try:
    with open(sys.argv[1], 'r') as cfile:
        content = cfile.read()
except:
    sys.exit("couldn't open file");

l = Topologies(content)
dot = l.network_parse(True, True)
fp = tempfile.NamedTemporaryFile()
fp.write(dot.encode())
fp.seek(0)
g = load_graph(fp.name, fmt='dot')

if len(sys.argv) == 3:
    if sys.argv[2] == "sfdp":
        pos = sfdp_layout(g)
    elif sys.argv[2] == "fr":
        pos = fruchterman_reingold_layout(g)
    elif sys.argv[2] == "arf":
        pos = arf_layout(g)
    elif sys.argv[2] == "rad":
        pos = radial_tree_layout(g)
    elif sys.argv[2] == "rand":
        pos = random_layout(g)
    else:
        sys.exit("no such layout algorithm");
else:
    pos = sfdp_layout(g);

vprops = { 'text': g.vertex_properties["label"] }

graph_draw(g, pos=pos, vprops=vprops, vertex_size=10, vertex_font_size=10,
vertex_text_position=1, vertex_aspect=1, vertex_anchor=0, edge_pen_width=1,
output_size=[4024,4024])
