int
topologies_definition_to_graph (void *n, void **g, char *e_text,
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

int
topologies_network_read_string (void *net, char *string,
	char *e_text, size_t e_size);

void
topologies_graph_print (void *g, FILE *stream, bool print_gate_nodes);

char *
topologies_graph_string (void *g, bool print_gate_nodes);

void
topologies_graph_string_free (char *string);

void
topologies_graph_destroy (void *g);
