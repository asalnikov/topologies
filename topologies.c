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
#include "topologies.h"
#include "errors.h"

static int
file_map (char *filename, char **addr, off_t *off, char *e_text, size_t e_size)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		return return_error(e_text, e_size, TOP_E_FOPEN, " %s: %s",
		                    filename, strerror(errno));
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		return return_error(e_text, e_size, TOP_E_FSTAT,
		                    " %s: %s", filename, strerror(errno));
	}

	*addr = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*addr == MAP_FAILED) {
		return return_error(e_text, e_size, TOP_E_FMMAP,
		                    " %s: %s", filename, strerror(errno));
	}

	close(fd);
	*off = st.st_size;
	return 0;
}

static void
file_close (char *addr, size_t len)
{
	munmap(addr, len);
}

static int
graph_eval_and_add_edge (graph_t *g, param_stack_t *p,
	name_stack_t *s, connection_wrapper_t *conn,
	char *e_text, size_t e_size)
{
	char *r_name_a = conn->ptr.conn->from;
	char *r_name_b = conn->ptr.conn->to;
	char *name_a, *name_b;
	int res;
	if ((res = eval_conn_name(p, r_name_a, &name_a, e_text, e_size)))
		return res;
	if ((res = eval_conn_name(p, r_name_b, &name_b, e_text, e_size))) {
		free(name_a);
		return res;
	}
	char *full_name_a = get_full_name(s, name_a, -1);
	if (!full_name_a) {
		free(name_a);
		free(name_b);
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	char *full_name_b = get_full_name(s, name_b, -1);
	if (!full_name_b) {
		free(name_a);
		free(name_b);
		free(full_name_a);
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	if (graph_add_edge_name(g, full_name_a, full_name_b)) {
		return_error(e_text, e_size, TOP_E_CONN,
			" %s %s", full_name_a, full_name_b);
		free(name_a);
		free(name_b);
		free(full_name_a);
		free(full_name_b);
		return TOP_E_CONN;
	}
	free(full_name_a);
	free(full_name_b);
	free(name_a);
	free(name_b);
	return 0;
}

static module_t *
find_module (network_definition_t *net, char *name)
{
	for (int i = 0; i < net->n_modules; i++) {
		if (strcmp(net->modules[i].name, name) == 0)
			return &net->modules[i];
	}
	return NULL;
}

static int
add_gate (graph_t *g, name_stack_t *s, char *name_s,
          gate_t *gate, int j, char *e_text, size_t e_size)
{
	int res;
	char *full_name = get_full_name(s, gate->name, j);
	if (!full_name)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (graph_add_node(g, full_name, NODE_GATE)) {
		free(full_name);
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	// TODO distinguish no mem and bad conn here:
	if ((res = graph_add_edge_name(g, full_name, name_s))) {
		if (res == TOP_E_CONN) {
			return_error(e_text, e_size, TOP_E_CONN,
				" %s %s", full_name, name_s);
			free(full_name);
			return TOP_E_CONN;
		} else {
			free(full_name);
			return return_error(e_text, e_size, res, "");
		}
	}
	free(full_name);
	return 0;
}

static int
expand_module (graph_t *g, module_t *module, network_definition_t *net,
	name_stack_t *s, param_stack_t *p, char *e_text, size_t e_size);

static int
enter_and_expand_module (network_definition_t *net, graph_t *g,
	name_stack_t *s, param_stack_t *p,
	submodule_t *smodule, int j,
	char *e_text, size_t e_size)
{
	int res;
	if (name_stack_enter(s, smodule->name, j))
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	module_t *module = find_module(net, smodule->module);
	if (module == NULL) {
		name_stack_leave(s);
		return return_error(e_text, e_size, TOP_E_NOMOD,
			": %s", smodule->module);
	}
	if ((res = expand_module(g, module, net, s, p, e_text, e_size))) {
		name_stack_leave(s);
		return res;
	}
	name_stack_leave(s);
	return 0;
}

static int
traverse_and_add_conns (connection_wrapper_t *c, graph_t *g,
	param_stack_t *p, name_stack_t *s, char *e_text, size_t e_size)
{
	int res;
	if (c->type == CONN_HAS_LOOP) {
		double tmp_d;
		if (param_stack_eval(p, c->ptr.loop->start, &tmp_d))
		{
			return return_error(e_text, e_size, TOP_E_EVAL,
				": %s", c->ptr.loop->start);
		}
		int start = lrint(tmp_d);
		if (param_stack_eval(p, c->ptr.loop->end, &tmp_d))
		{
			return return_error(e_text, e_size, TOP_E_EVAL,
				": %s", c->ptr.loop->end);
		}
		int end = lrint(tmp_d);
		if (start > end) {
			return return_error(e_text, e_size, TOP_E_LOOP,
				"%d > %d\n", start, end);
		}
		for (int j = start; j <= end; j++) {
			if (param_stack_enter_val(p, c->ptr.loop->loop, j))
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			if ((res = traverse_and_add_conns(c->ptr.loop->conn, g,
				p, s, e_text, e_size)))
			{
				return res;
			}
			param_stack_leave(p);
		}
	} else {
		if ((res = graph_eval_and_add_edge(g, p, s, c, e_text, e_size)))
			return res;
	}
	return 0;
}

static int
expand_module (graph_t *g, module_t *module, network_definition_t *net,
	name_stack_t *s, param_stack_t *p, char *e_text, size_t e_size)
{
	int res;
	for (int i = 0; i < module->n_params; i++) {
		if ((res = param_stack_enter(p, &module->params[i], e_text, e_size)))
			return res;
	}
	if (module->submodules == NULL) {
		/* add gates and connect them to the node */
		char *name_s = name_stack_name(s);
		if (!name_s)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		if (graph_add_node(g, name_s, NODE_NODE)) {
			free(name_s);
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		for (int i = 0; i < module->n_gates; i++) {
			double size_d;
			if (param_stack_eval(p, module->gates[i].size, &size_d)) {
				return return_error(e_text, e_size, TOP_E_EVAL,
					": %s", module->gates[i].size);
			}
			int size = lrint(size_d);
			if (size == 0) {
				if ((res = add_gate(g, s, name_s,
					&module->gates[i], -1, e_text, e_size)))
				{
					free(name_s);
					return res;
				}
			} else {
				for (int j = 0; j < size; j++) {
					if ((res = add_gate(g, s, name_s,
						&module->gates[i], j, e_text,
						e_size)))
					{
						free(name_s);
						return res;
					}
				}
			}
		}
		free(name_s);
	} else {
		/* add gates, add submodules, add connections */
		for (int i = 0; i < module->n_gates; i++) {
			double size_d;
			if (param_stack_eval(p, module->gates[i].size, &size_d)) {
				return return_error(e_text, e_size, TOP_E_EVAL,
					": %s", module->gates[i].size);
			}
			int size = lrint(size_d);
			if (size == 0) {
				char *full_name = get_full_name(s,
					module->gates[i].name, -1);
				if (!full_name)
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				if (graph_add_node(g, full_name, NODE_GATE))
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				free(full_name);
			} else {
				for (int j = 0; j < size; j++) {
					char *full_name = get_full_name(s,
						module->gates[i].name, j);
					if (!full_name)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					if (graph_add_node(g, full_name, NODE_GATE))
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					free(full_name);
				}
			}
		}
		for (int i = 0; i < module->n_submodules; i++) {
			submodule_t smodule = module->submodules[i];
			for (int i = 0; i < smodule.n_params; i++) {
				if ((res = param_stack_enter(p,
					&smodule.params[i], e_text, e_size)))
				{
					return res;
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
					if ((res = enter_and_expand_module(net, g, s,
						p, &smodule, j, e_text,
						e_size)))
					{
						return res;
					}
				}
			} else {
				if ((res = enter_and_expand_module(net, g, s,
					p, &smodule, -1, e_text, e_size)))
				{
					return res;
				}
			}
			for (int i = 0; i < smodule.n_params; i++) {
				param_stack_leave(p);
			}
		}
		for (int i = 0; i < module->n_connections; i++) {
			if ((res =
				traverse_and_add_conns(&module->connections[i],
				g, p, s, e_text, e_size)))
			{
				return res;
			}
		}
	}
	for (int i = 0; i < module->n_params; i++) {
		param_stack_leave(p);
	}
	return 0;
}

int
topologies_definition_to_graph (void *v, void **r_g, char *e_text, size_t e_size)
{
	network_definition_t *net = (network_definition_t *) v;
	if (net->network == NULL)
		return return_error(e_text, e_size, TOP_E_NONET, ""); 
	module_t *root_module = find_module(net, net->network->module);
	if (root_module == NULL) {
		return return_error(e_text, e_size, TOP_E_NOMOD, " %s",
			net->network->module); 
	}
	graph_t *g;
	name_stack_t *s;
	param_stack_t *p;
	int res;

	g = topologies_graph_create();
	if (!g)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	s = name_stack_create("network");
	if (!s) {
		free(g->nodes);
		free(g);
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	p = param_stack_create();
	if (!p) {
		free(g->nodes);
		free(g);
		free(s->name);
		free(s);
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	for (int i = 0; i < net->network->n_params; i++) {
		if ((res = param_stack_enter(p, &net->network->params[i],
			e_text, e_size)))
		{
			return res;
		}
	}
	if ((res = expand_module(g, root_module, net, s, p, e_text, e_size)))
		return res;

	for (int i = 0; i < net->network->n_params; i++) {
		param_stack_leave(p);
	}
	param_stack_destroy(p);
	free(s->name);
	free(s);
	*r_g = (void *) g;
	return 0;
}

static int
graph_gate_neighbors (node_t *gate, node_t **node_1, node_t **node_2,
	char *e_text, size_t e_size)
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
			return return_error(e_text, e_size, TOP_E_BADGATE,
				" %s", gate->name);
		}
		l = l->next;
	}
	return 0;
}

static int
graph_traverse_gate_neighbors (node_t *gate, node_t *node_a, node_t **r_node,
	char *e_text, size_t e_size)
{
	int res;
	node_t *node_ta, *node_tb, *node_prev;
	if (node_a != NULL) {
		node_prev = gate;
		while (node_a->type != NODE_NODE) {
			node_a->type = NODE_GATE_VISITED;
			if ((res = graph_gate_neighbors(node_a, &node_ta, &node_tb,
				e_text, e_size)))
			{
				return res;
			}
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

	*r_node = node_a;
	return 0;
}

static int
graph_gate_connects_whom (node_t *gate, node_t **n_node_1, node_t **n_node_2,
	char *e_text, size_t e_size)
{
	int res;
	node_t *node_a, *node_b;
	gate->type = NODE_GATE_VISITED;
	if ((res = graph_gate_neighbors(gate, &node_a, &node_b, e_text, e_size)))
		return res;
	if ((res = graph_traverse_gate_neighbors(gate, node_a, n_node_1,
		e_text, e_size)))
	{
		return res;
	}
	if ((res = graph_traverse_gate_neighbors(gate, node_b, n_node_2,
		e_text, e_size)))
	{
		return res;
	}
	return 0;
}

int
topologies_graph_compact (void *v, char *e_text, size_t e_size)
{
	int res;
	graph_t *g = (graph_t *) v;
	node_list_t *l, *l_next;
	/* connect nodes with nodes */
	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type != NODE_GATE) continue;

		node_t *n_node_1 = NULL, *n_node_2 = NULL;
		if ((res = graph_gate_connects_whom(&g->nodes[i],
			&n_node_1, &n_node_2, e_text, e_size)))
		{
			return res;
		}
		if ((n_node_1 != NULL) && (n_node_2 != NULL) &&
			(n_node_1->n > n_node_2->n))
		{
			if (graph_add_edge_ptr(n_node_1, n_node_2))
				return return_error(e_text, e_size, TOP_E_ALLOC, "");

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
	return 0;
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

void
topologies_network_destroy (void *v)
{
	network_definition_t *n = (network_definition_t *) v;
	if (n == NULL) return;

	if (n->modules) {
		for (int i = 0; i < n->n_modules; i++) {
			if (n->modules[i].params) {
				for (int j = 0; j < n->modules[i].n_params; j++) {
					free(n->modules[i].params[j].name);
					free(n->modules[i].params[j].value);
				}
				free(n->modules[i].params);
			}
			if (n->modules[i].submodules) {
				for (int j = 0; j < n->modules[i].n_submodules; j++) {
					free(n->modules[i].submodules[j].name);
					free(n->modules[i].submodules[j].module);
					free(n->modules[i].submodules[j].size);
					if (n->modules[i].submodules[j].params) {
						for (int k = 0; 
						     k < n->modules[i].submodules[j].n_params;
						     k++)
						{
							free(n->modules[i].submodules[j].params[k].name);
							free(n->modules[i].submodules[j].params[k].value);
						}
						free(n->modules[i].submodules[j].params);
					}
				}
				free(n->modules[i].submodules);
			}
			if (n->modules[i].gates) {
				for (int j = 0; j < n->modules[i].n_gates; j++) {
					free(n->modules[i].gates[j].name);
					free(n->modules[i].gates[j].size);
				}
				free(n->modules[i].gates);
			}
			if (n->modules[i].connections) {
				for (int j = 0; j < n->modules[i].n_connections; j++) {
					free_connection(&n->modules[i].connections[j]);
				}
				free(n->modules[i].connections);
			}
			free(n->modules[i].name);
		}
		free(n->modules);
	}
	if (n->network) {
		if (n->network->params) {
			for (int i = 0; i < n->network->n_params; i++) {
				free(n->network->params[i].name);
				free(n->network->params[i].value);
			}
		}
		free(n->network->module);
		free(n->network);
	}
}

int
topologies_network_init (void **rnet, char *e_text, size_t e_size)
{
	network_definition_t *net = calloc(1, sizeof(network_definition_t));
	if (net == NULL)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	*rnet = (void *) net;
	return 0;
}

int
topologies_network_read_file (void *net, char *filename, char *e_text, size_t e_size)
{
	char *addr = NULL;
	off_t file_size;
	int res;
	if ((res = file_map(filename, &addr, &file_size, e_text, e_size)))
		return res;
	res = json_read_file(addr, file_size, net, e_text, e_size);
	file_close(addr, file_size);
	return res;
}
