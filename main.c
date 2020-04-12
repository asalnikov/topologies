#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "tinyexpr.h"

#include "parser.h"
#include "name_stack.h"
#include "param_stack.h"
#include "graph.h"
#include "defs.h"

/* TODO
 * param vectors
 * compact network form
 * rewrite malloc
 */

static void
error (const char * errmsg, ...)
{
	va_list args;
	va_start(args, errmsg);
	fprintf(stderr, errmsg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static off_t
read_file (int argc, char *argv[], char **addr)
{
	if (argc != 2)
		error("usage: %s config.json\n", argv[0]);

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		error("could not open %s: %s\n", argv[1], strerror(errno));

	struct stat st;
	if (fstat(fd, &st) < 0)
		error("could not stat %s: %s\n", argv[1], strerror(errno));

	*addr = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*addr == MAP_FAILED)
		error("could not mmap %s: %s\n", argv[1], strerror(errno));

	close(fd);
	return st.st_size;
}

static void
graph_eval_and_add_edge (graph_t *g, param_stack_t *p,
                         name_stack_t *s,
                         connection_wrapper_t *conn)
{
	char *r_name_a = conn->ptr.conn->from;
	char *r_name_b = conn->ptr.conn->to;
	char *name_a = eval_conn_name(p, r_name_a);
	char *name_b = eval_conn_name(p, r_name_b);
	if (name_a == NULL)
		error("could not evaluate %s\n", r_name_a);
	if (name_b == NULL)
		error("could not evaluate %s\n", r_name_b);
	char *full_name_a = get_full_name(s, name_a, -1);
	char *full_name_b = get_full_name(s, name_b, -1);
	if (graph_add_edge_name(g, full_name_a, full_name_b) < 0) {
		error("could not connect %s and %s\n",
			full_name_a, full_name_b);
	}
	free(full_name_a);
	free(full_name_b);
	free(name_a);
	free(name_b);
}

static module_t *
find_module (network_definition_t *net, char *name)
{
	for (int i = 0; i < net->n_modules; i++)
		if (strcmp(net->modules[i].name, name) == 0)
			return &net->modules[i];
	return NULL;
}

static void
add_gate (graph_t *g, name_stack_t *s, char *name_s,
          gate_t *gate, int j)
{
	char *full_name = get_full_name(s, gate->name, j);
	graph_add_node(g, full_name, NODE_GATE);
	if (graph_add_edge_name(g, full_name, name_s) < 0) {
		error("could not connect %s and %s\n",
			full_name, name_s);
	}
	free(full_name);
}

static void
expand_module (graph_t *g, module_t *module, network_definition_t *net,
               name_stack_t *s, param_stack_t *p);

static void
enter_and_expand_module(network_definition_t *net, graph_t *g,
                        name_stack_t *s, param_stack_t *p,
                        submodule_t *smodule, int j)
{
	name_stack_enter(s, smodule->name, j);
	module_t *module = find_module(net, smodule->module);
	if (module == NULL) {
		error("no such module: %s\n", smodule->module);
	}
	expand_module(g, module, net, s, p);
	name_stack_leave(s);
}

static void
traverse_and_add_conns(connection_wrapper_t *c, graph_t *g,
                       param_stack_t *p, name_stack_t *s)
{
	if (c->type == CONN_HAS_LOOP) {
		double tmp_d;
		if (param_stack_eval(p, c->ptr.loop->start, &tmp_d) < 0)
		{
			error("could not evaluate %s\n",
			      c->ptr.loop->start);
		}
		int start = lrint(tmp_d);
		if (param_stack_eval(p, c->ptr.loop->end, &tmp_d) < 0)
		{
			error("could not evaluate %s\n",
			      c->ptr.loop->end);
		}
		int end = lrint(tmp_d);
		if (start > end) {
			error("%d > %d\n", start, end);
		}
		for (int j = start; j <= end; j++) {
			param_stack_enter_val(p, c->ptr.loop->loop, j);
			traverse_and_add_conns(c->ptr.loop->conn, g, p, s);
			param_stack_leave(p);
		}
	} else {
		graph_eval_and_add_edge(g, p, s, c);
	}
}

static void
expand_module (graph_t *g, module_t *module, network_definition_t *net,
               name_stack_t *s, param_stack_t *p)
{
	for (int i = 0; i < module->n_params; i++) {
		if (param_stack_enter(p, &module->params[i]) < 0) {
			error("could not evaluate %s\n", module->params[i].value);
		}
	}
	if (module->submodules == NULL) {
		/* add gates and connect them to the node */
		char *name_s = name_stack_name(s);
		graph_add_node(g, name_s, NODE_NODE);
		for (int i = 0; i < module->n_gates; i++) {
			double size_d;
			if (param_stack_eval(p, module->gates[i].size, &size_d) < 0) {
				error("could not evaluate %s\n", module->gates[i].size);
			}
			int size = lrint(size_d);
			if (size == 0) {
				add_gate(g, s, name_s, &module->gates[i], -1);
			} else {
				for (int j = 0; j < size; j++) {
					add_gate(g, s, name_s, &module->gates[i], j);
				}
			}
		}
		free(name_s);
	} else {
		/* add gates, add submodules, add connections */
		for (int i = 0; i < module->n_gates; i++) {
			double size_d;
			if (param_stack_eval(p, module->gates[i].size, &size_d) < 0) {
				error("could not evaluate %s\n", module->gates[i].size);
			}
			int size = lrint(size_d);
			if (size == 0) {
				char *full_name = get_full_name(s,
					module->gates[i].name, -1);
				graph_add_node(g, full_name, NODE_GATE);
				free(full_name);
			} else {
				for (int j = 0; j < size; j++) {
					char *full_name = get_full_name(s,
						module->gates[i].name, j);
					graph_add_node(g, full_name, NODE_GATE);
					free(full_name);
				}
			}
		}
		for (int i = 0; i < module->n_submodules; i++) {
			submodule_t smodule = module->submodules[i];
			for (int i = 0; i < smodule.n_params; i++) {
				if (param_stack_enter(p, &smodule.params[i]) < 0) {
					error("could not evaluate %s\n",
					      smodule.params[i].value);
				}
			}
			int size;
			if (smodule.size == NULL) {
				size = 0;
			} else {
				sscanf(smodule.size, "%d", &size);
			}
			if (size > 0) {
				for (int j = 0; j < size; j++) {
					enter_and_expand_module(net, g, s,
						p, &smodule, j);
				}
			} else {
				enter_and_expand_module(net, g, s,
					p, &smodule, -1);
			}
			for (int i = 0; i < smodule.n_params; i++) {
				param_stack_leave(p);
			}
		}
		for (int i = 0; i < module->n_connections; i++) {
			traverse_and_add_conns(&module->connections[i], g, p, s);
		}
	}
	for (int i = 0; i < module->n_params; i++) {
		param_stack_leave(p);
	}
}

static graph_t *
definition_to_graph (network_definition_t *net)
{
	module_t *root_module = find_module(net, net->network->module);
	if (root_module == NULL) {
		error("no such module: %s\n", root_module);
	}
	graph_t *g = graph_create();
	name_stack_t *s = name_stack_create("network");
	param_stack_t *p = param_stack_create();
	for (int i = 0; i < net->network->n_params; i++) {
		if (param_stack_enter(p, &net->network->params[i]) < 0) {
			error("could not evaluate %s\n",
			      net->network->params[i].value);
		}
	}
	expand_module(g, root_module, net, s, p);
	for (int i = 0; i < net->network->n_params; i++) {
		param_stack_leave(p);
	}
	param_stack_destroy(p);
	free(s->name);
	free(s);
	return g;
}

static void
graph_gate_neighbors (node_t *gate, node_t **node_1, node_t **node_2)
{
	node_list_t *l = gate->adj;
	*node_1 = NULL;
	*node_2 = NULL;
	while (l != NULL) {
		if (*node_1 == NULL) {
			*node_1 = l->node;
		} else if (*node_2 == NULL) {
			*node_2 = l->node;
		} else {
			error("gate %s is connected more than two times",
				gate->name);
		}
		l = l->next;
	}
	/* TODO strict mode ?
	if ((*node_1 == NULL) || (*node_2 == NULL))
		error("gate %s is connected less than two times", gate->name);
	*/
}

static node_t *
graph_traverse_gate_neighbors (node_t *gate, node_t *node_a)
{
	node_t *node_ta, *node_tb, *node_prev;
	if (node_a != NULL) {
		node_prev = gate;
		while (node_a->type != NODE_NODE) {
			node_a->type = NODE_GATE_VISITED;
			graph_gate_neighbors(node_a, &node_ta, &node_tb);
			if ((node_ta == NULL) || (node_tb == NULL)) break;
			if (node_ta != node_prev) {
				node_prev = node_a;
				node_a = node_ta;
			} else {
				node_prev = node_a;
				node_a = node_tb;
			}
		}
	}

	return node_a;
}

static void
graph_gate_connects_whom (node_t *gate, node_t **n_node_1, node_t **n_node_2)
{
	node_t *node_a, *node_b;
	gate->type = NODE_GATE_VISITED;
	graph_gate_neighbors(gate, &node_a, &node_b);
	*n_node_1 = graph_traverse_gate_neighbors(gate, node_a);
	*n_node_2 = graph_traverse_gate_neighbors(gate, node_b);
}

static void
graph_compact (graph_t *g)
{
	node_list_t *l, *l_next;
	/* connect nodes with nodes */
	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type != NODE_GATE) continue;

		node_t *n_node_1 = NULL, *n_node_2 = NULL;
		graph_gate_connects_whom(&g->nodes[i], &n_node_1, &n_node_2);

		if ((n_node_1 != NULL) && (n_node_2 != NULL) &&
			(n_node_1->n > n_node_2->n))
		{
			(void) graph_add_edge_ptr(n_node_1, n_node_2);
		}
	}
	/* disconnect gates */
	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type != NODE_NODE) {
			l = g->nodes[i].adj;
			while (l != NULL) {
				l_next = l->next;
				free(l);
				l = l_next;
			}
			g->nodes[i].adj = NULL;
		} else {
			l = g->nodes[i].adj;
			node_list_t **prev = &g->nodes[i].adj;
			while (l != NULL) {
				l_next = l->next;
				if (l->node->type != NODE_NODE) {
					free(l);
					*prev = l_next;
				} else {
					prev = &l->next;
				}
				l = l_next;
			}
		}
	}
}

static void
free_connection (connection_wrapper_t *c)
{
	if (c->type == CONN_HAS_CONN) {
		free(c->ptr.conn->from);
		free(c->ptr.conn->to);
		free(c->ptr.conn);
	} else {
		free_connection(c->ptr.loop->conn);
		free(c->ptr.loop->conn);
		free(c->ptr.loop->loop);
		free(c->ptr.loop->start);
		free(c->ptr.loop->end);
		free(c->ptr.loop);
	}
}

static void
network_destroy (network_definition_t *n)
{
	for (int i = 0; i < n->n_modules; i++) {
		for (int j = 0; j < n->modules[i].n_params; j++) {
			free(n->modules[i].params[j].name);
			free(n->modules[i].params[j].value);
		}
		free(n->modules[i].params);
		for (int j = 0; j < n->modules[i].n_submodules; j++) {
			free(n->modules[i].submodules[j].name);
			free(n->modules[i].submodules[j].module);
			free(n->modules[i].submodules[j].size);
			for (int k = 0; 
			     k < n->modules[i].submodules[j].n_params;
			     k++)
			{
				free(n->modules[i].submodules[j].params[k].name);
				free(n->modules[i].submodules[j].params[k].value);
			}
			free(n->modules[i].submodules[j].params);
		}
		free(n->modules[i].submodules);
		for (int j = 0; j < n->modules[i].n_gates; j++) {
			free(n->modules[i].gates[j].name);
			free(n->modules[i].gates[j].size);
		}
		free(n->modules[i].gates);
		for (int j = 0; j < n->modules[i].n_connections; j++) {
			free_connection(&n->modules[i].connections[j]);
		}
		free(n->modules[i].connections);
		free(n->modules[i].name);
	}
	free(n->modules);
	for (int i = 0; i < n->network->n_params; i++) {
		free(n->network->params[i].name);
		free(n->network->params[i].value);
	}
	free(n->network->module);
	free(n->network);
}

int
main (int argc, char *argv[])
{
	char *addr = NULL;
	network_definition_t network_definition = { 0 };

	off_t file_size = read_file(argc, argv, &addr);
	int res = json_read_file(addr, file_size, &network_definition);
	if (res < 0)
		error("%s: invalid JSON data: %d\n", argv[1], res);

	graph_t *gg = definition_to_graph(&network_definition);
	graph_print(gg, stdout, true);
	graph_compact(gg);
	graph_print(gg, stdout, false);
	graph_destroy(gg);

	network_destroy(&network_definition);

	exit(EXIT_SUCCESS);
}
