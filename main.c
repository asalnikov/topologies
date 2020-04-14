#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "graph.h"
#include "topologies.h"

/* TODO
 * param vectors
 * compact network form
 * rewrite malloc
 * free memory on error
 */

static void
error (const char * errmsg, ...)
{
	va_list args;
	va_start(args, errmsg);
	fprintf(stderr, errmsg, args);
	va_end(args);
	fflush(stdout);
	exit(EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
	void *net = topologies_network_init();
	int res;

	if (argc < 2)
		error("usage: %s config.json [config_2.json ...]\n", argv[0]);

	for (int i = 1; i < argc; i++) {
		if ((res = topologies_network_read_file(net, argv[i])) < 0) {
			error("%s: invalid JSON data: %d\n", argv[i], res);
		}
	}

	void *graph = topologies_definition_to_graph(net);
	topologies_graph_print(graph, stdout, true);
	topologies_graph_compact(graph);
	topologies_graph_print(graph, stdout, false);
	topologies_graph_destroy(graph);

	topologies_network_destroy(net);

	exit(EXIT_SUCCESS);
}
