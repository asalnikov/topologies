#ifndef TOPOLOGIES_H
# define TOPOLOGIES_H

int
topologies_definition_to_graph (void *v, void **r_g, char *e_text,
	size_t e_size);

int
topologies_graph_compact (void **g, char *e_text, size_t e_size);

void
topologies_network_destroy (void *n);

int
topologies_network_init (void **rnet, char *e_text, size_t e_size);

int
topologies_network_read_file (void *net, char *filename, char *e_text,
	size_t e_size);

graph_t *
topologies_graph_create (void);

void
topologies_graph_print (graph_t *g, FILE *stream, bool print_gate_nodes);

char *
topologies_graph_string (graph_t *g, bool print_gate_nodes);

void
topologies_graph_string_free (char *string);

void
topologies_graph_destroy (graph_t *g);

#endif
