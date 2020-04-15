#ifndef TOPOLOGIES_H
# define TOPOLOGIES_H

int
topologies_definition_to_graph (void *v, void **r_g, char *e_text,
	size_t e_size);

int
topologies_graph_compact (void *g, char *e_text, size_t e_size);

void
topologies_network_destroy (void *n);

int
topologies_network_init (void **rnet, char *e_text, size_t e_size);

int
topologies_network_read_file (void *net, char *filename, char *e_text,
	size_t e_size);

#endif
