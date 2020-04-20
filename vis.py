#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import tempfile
import argparse
import graph_tool
import graph_tool.draw
from wrapper import Topologies

parser = argparse.ArgumentParser()
parser.add_argument('file', nargs='+', type=argparse.FileType('r'))
parser.add_argument('--layout', nargs='?', const='sfdp', default='sfdp',
    choices=["sfdp","fr","arf","rand"])

args = parser.parse_args()
content = ""
for f in args.file:
    content += f.read()

l = Topologies(content)
dot = l.network_parse(True, False)
fp = tempfile.NamedTemporaryFile()
fp.write(dot.encode())
fp.seek(0)
g = graph_tool.load_graph(fp.name, fmt='dot')

if args.layout == "fr":
    pos = graph_tool.draw.fruchterman_reingold_layout(g)
elif args.layout == "arf":
    pos = graph_tool.draw.arf_layout(g)
elif args.layout == "rand":
    pos = graph_tool.draw.random_layout(g)
else:
    pos = graph_tool.draw.sfdp_layout(g)

vprops = { 'text': g.vertex_properties["label"] }

graph_tool.draw.graph_draw(g, pos=pos, vprops=vprops, vertex_size=10,
vertex_font_size=10, vertex_text_position=1, vertex_aspect=1, vertex_anchor=0,
edge_pen_width=1, output_size=[4024,4024])
