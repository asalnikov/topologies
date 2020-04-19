#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import ctypes
import sys

class Topologies:
    network = ctypes.c_void_p(None)
    graph = ctypes.c_void_p(None)

    def __init__(self, definition):
        self.library = ctypes.CDLL("./libtopologies.so")
        self.e_size = 1024
        self.e_buf = ctypes.create_string_buffer(self.e_size)
        self.definition = ctypes.c_char_p(definition.encode('utf-8'))
        
        self.library.topologies_network_init.argtypes = \
            [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
        self.library.topologies_network_init.restype = ctypes.c_int
        
        self.library.topologies_network_read_string.argtypes = \
            [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
        self.library.topologies_network_read_string.restype = ctypes.c_int

        self.library.topologies_definition_to_graph.argtypes = \
            [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
        self.library.topologies_definition_to_graph.restype = ctypes.c_int

        self.library.topologies_graph_string.argtypes = \
            [ctypes.c_void_p, ctypes.c_bool]
        self.library.topologies_graph_string.restype = ctypes.c_void_p

        self.library.topologies_graph_string_free.argtypes = [ctypes.c_void_p]
        self.library.topologies_network_destroy.argtypes = [ctypes.c_void_p]
        self.library.topologies_graph_destroy.argtypes = [ctypes.c_void_p]

    def network_parse(self):
        # TODO throw if error
        self.library.topologies_network_init(ctypes.byref(self.network),
            self.e_buf, ctypes.sizeof(self.e_buf))
        self.library.topologies_network_read_string(self.network,
            self.definition, self.e_buf, ctypes.sizeof(self.e_buf))

        self.library.topologies_definition_to_graph(self.network,
            ctypes.byref(self.graph), self.e_buf, ctypes.sizeof(self.e_buf))
        self.dot_ptr = self.library.topologies_graph_string(self.graph, True)
        value = str(ctypes.cast(self.dot_ptr, ctypes.c_char_p).value, 'utf-8')

        self.library.topologies_graph_string_free(self.dot_ptr)
        self.library.topologies_network_destroy(self.network)
        self.library.topologies_graph_destroy(self.graph)
        print(value)


if __name__ == '__main__':
    with open(sys.argv[1], 'r') as cfile:
        content = cfile.read()

    l = Topologies(content)
    l.network_parse()
