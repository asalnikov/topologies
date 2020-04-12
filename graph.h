#ifndef GRAPH_H
# define GRAPH_H

#include "defs.h"

graph_t *
graph_create (void);

node_t *
graph_add_node (graph_t *g, char *name, node_type type);

int
graph_add_edge_ptr (node_t *node_a, node_t *node_b);

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b);

void
graph_print (graph_t *g, FILE *stream, bool print_gate_nodes);

void
graph_destroy (graph_t *g);

#endif
