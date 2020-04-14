#ifndef TOPOLOGIES_H
# define TOPOLOGIES_H

void *
topologies_definition_to_graph (void *net);

void
topologies_graph_compact (void *g);

void
topologies_network_destroy (void *n);

void *
topologies_network_init (void);

int
topologies_network_read_file (void *net, char *filename);

/*
typedef enum topologies_error {

} topologies_error_t;
*/

#endif
