#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "graph.h"
#include "topologies.h"

/* TODO
 * comments!
 * distinguish nodes and routers
 *
 * rewrite malloc ?
 * connect to the node's next free gate ?
 * pass submodule's module as a param => text variables ?
 *
 * compact network form - in the end
 * exceptions / irregularities - in the end
 */

/* TinyExpr supports addition (+), subtraction/negation (-), multiplication (*),
 * division (/), exponentiation (^) and modulus (%).
 * 
 * The following C math functions are also supported:
 * abs (calls to fabs), acos, asin, atan, atan2, ceil, cos, cosh, exp, floor,
 * ln, log (calls to log10), log10, pow, sin, sinh, sqrt, tan, tanh.
 *
 * The following functions are also built-in and provided by TinyExpr:
 * fac (factorials e.g. fac 5 == 120)
 * ncr (combinations e.g. ncr(6,2) == 15)
 * npr (permutations e.g. npr(6,2) == 30)
 *
 * Also, the following constants are available: pi, e.
*/


int
main (int argc, char *argv[])
{
	void *net;
	int res;
	int e_size = 1024;
	char e_text[1024] = "";

	if (argc < 2) {
		printf("usage: %s config.json [config_2.json ...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (topologies_network_init(&net, e_text, e_size)) {
		printf("%s\n", e_text);
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i < argc; i++) {
		if ((res = topologies_network_read_file(net, argv[i], e_text, e_size)))
		{
			printf("%s\n", e_text);
			exit(EXIT_FAILURE);
		}
	}

	void *graph;
	if (topologies_definition_to_graph(net, &graph, e_text, e_size)) {
		printf("%s\n", e_text);
		exit(EXIT_FAILURE);
	}
	topologies_graph_print(graph, stdout, true);
	if (topologies_graph_compact(&graph, e_text, e_size)) {
		printf("%s\n", e_text);
		exit(EXIT_FAILURE);
	}
	topologies_graph_print(graph, stdout, false);
	topologies_graph_destroy(graph);

	topologies_network_destroy(net);

	exit(EXIT_SUCCESS);
}
