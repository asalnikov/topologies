#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "graph.h"
#include "topologies.h"

/* TODO
 * free memory on error
 * error propagation
 * if in connections
 * pass submodule's module as a param
 * distinguish simple and compound modules
 * distinguish nodes and routers
 * cartesian product https://en.wikipedia.org/wiki/Cartesian_product_of_graphs
 ** compact the subgraph before computing product
 *
 * param vectors ?
 * rewrite malloc ?
 * strict mode, error on unconnected gates ?
 * connect to next free gate ?
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
