#ifndef GRAPH_H
# define GRAPH_H

#include "defs.h"

int
graph_add_node (graph_t *g, char *name, node_type type);

node_t *
graph_find_node (graph_t *g, char *name);

int
graph_add_edge_ptr (node_t *node_a, node_t *node_b);

bool
graph_are_adjacent (node_t *node_a, node_t *node_b);

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b);

#endif
