#ifndef GRAPH_H
# define GRAPH_H

#include "defs.h"

int
graph_add_node (graph_t *g, char *name, node_type type, char *attrs);

int
graph_find_node (graph_t *g, char *name);

int
graph_add_edge_id (graph_t *g, int node_a, int node_b, char *attrs);

bool
graph_are_adjacent (node_t *node_a, node_t *node_b);

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b, char *attrs);

graph_t *
graph_create (void);

#endif
