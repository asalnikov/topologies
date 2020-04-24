#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
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
add_auto_gate (graph_t *g, int *r_n_node, char *node_name, name_stack_t *s)
{
	int res;
	int j;
	int n_node = *r_n_node;
	if ((res = name_stack_enter(s, node_name, -1))) return res;

	char *auto_name = malloc(18); /* "_auto[2147483647]" */
	for (j = 0; j < INT_MAX; j++) {
		sprintf(auto_name, "_auto[%d]", j);
		bool seen = false;
		char *full_name = get_full_name(s, auto_name, -1);
		for (int k = 0; k < g->nodes[n_node].n_adj; k++) {
			if (strcmp(g->nodes[g->nodes[n_node].adj[k].n].name,
				full_name) == 0)
			{
				seen = true;
			}
		}
		free(full_name);
		if (!seen) break;
	}
	char *full_name = get_full_name(s, auto_name, -1);
	if ((res = graph_add_node(g, full_name, NODE_GATE))) return res;
	*r_n_node = graph_find_node(g, full_name);
	graph_add_edge_id(g, n_node, *r_n_node);
	free(full_name);
	free(auto_name);
	name_stack_leave(s);
	return 0;
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

	int n_node_a = graph_find_node(g, full_name_a);
	int n_node_b = graph_find_node(g, full_name_b);
	if (n_node_a < 0)
		return return_error(e_text, e_size, TOP_E_CONN,
			" %s %s", full_name_a, full_name_b);
	if (n_node_b < 0)
		return return_error(e_text, e_size, TOP_E_CONN,
			" %s %s", full_name_a, full_name_b);
	if (g->nodes[n_node_a].type == NODE_NODE)
		if (add_auto_gate(g, &n_node_a, name_a, s))
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (g->nodes[n_node_b].type == NODE_NODE)
		if (add_auto_gate(g, &n_node_b, name_b, s))
			return return_error(e_text, e_size, TOP_E_ALLOC, "");

	if (graph_add_edge_id(g, n_node_a, n_node_b)) {
		return_error(e_text, e_size, TOP_E_CONN,
			" %s %s", full_name_a, full_name_b);
		free(name_a);
		free(name_b);
		free(full_name_a);
		free(full_name_b);
		return TOP_E_CONN;
	}
	free(name_a);
	free(name_b);
	free(full_name_a);
	free(full_name_b);
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
	submodule_plain_t *smodule, int j,
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
		if ((res = param_stack_eval(p, c->ptr.loop->start, &tmp_d,
			e_text, e_size)))
		{
			return res;
		}
		int start = lrint(tmp_d);
		if ((res = param_stack_eval(p, c->ptr.loop->end, &tmp_d,
			e_text, e_size)))
		{
			return res;
		}
		int end = lrint(tmp_d);
		if (start > end) {
			return return_error(e_text, e_size, TOP_E_LOOP,
				"%d > %d\n", start, end);
		}
		for (int j = start; j < end; j++) {
			if (param_stack_enter_val(p, c->ptr.loop->loop, j))
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			if ((res = traverse_and_add_conns(c->ptr.loop->conn, g,
				p, s, e_text, e_size)))
			{
				return res;
			}
			param_stack_leave(p);
		}
	} else if (c->type == CONN_HAS_COND) {
		double tmp_d;
		if ((res = param_stack_eval(p, c->ptr.cond->condition, &tmp_d,
			e_text, e_size)))
		{
			return res;
		}
		int condition = lrint(tmp_d);
		if (condition) {
			if ((res = traverse_and_add_conns(c->ptr.cond->conn_then,
				g, p, s, e_text, e_size)))

			{
				return res;
			}
		} else if (c->ptr.cond->conn_else) {
			if ((res = traverse_and_add_conns(c->ptr.cond->conn_else,
				g, p, s, e_text, e_size)))
			{
				return res;
			}
		}
	} else {
		if ((res = graph_eval_and_add_edge(g, p, s, c, e_text, e_size)))
			return res;
	}
	return 0;
}

static int
graphs_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size)
{
	int res;
	int name_len;
	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	int name_buf_neigh_cap = name_buf_blk;
	char *name_buf_neigh = malloc(name_buf_neigh_cap);
	if (!name_buf_neigh)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");


	for (int i = 0; i < g_a->n_nodes; i++) {
		if (g_a->nodes[i].type != NODE_NODE) continue;
		for (int j = 0; j < g_b->n_nodes; j++) {
			if (g_b->nodes[j].type != NODE_NODE) continue;

			name_len = strlen(g_a->nodes[i].name) +
				strlen(g_b->nodes[j].name) + 4;
			if (name_buf_cap < name_len) {
				name_buf_cap = (1 + name_len / name_buf_blk) *
					name_buf_blk;
				name_buf = realloc(name_buf, name_buf_cap);
				if (!name_buf) {
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
			}

			sprintf(name_buf, "(%s,%s)", g_a->nodes[i].name,
				g_b->nodes[j].name);
			if (graph_add_node(g_prod, name_buf, NODE_NODE))
				return return_error(e_text, e_size, TOP_E_ALLOC, "");

			for (int k = 0; k < g_a->nodes[i].n_adj; k++) {
				if (g_a->nodes[g_a->nodes[i].adj[k].n].type != NODE_GATE)
					continue;
				name_len = strlen(name_buf) +
					strlen(g_a->nodes[g_a->nodes[i].adj[k].n].name) + 2;
				if (name_buf_neigh_cap < name_len) {
					name_buf_neigh_cap = (1 + name_len / name_buf_blk) *
						name_buf_blk;
					name_buf_neigh = realloc(name_buf_neigh,
						name_buf_neigh_cap);
					if (!name_buf_neigh) {
						return return_error(e_text, e_size,
							TOP_E_ALLOC, "");
					}
				}
				sprintf(name_buf_neigh, "%s.%s", name_buf,
					g_a->nodes[g_a->nodes[i].adj[k].n].name);
				if (graph_add_node(g_prod, name_buf_neigh, NODE_GATE))
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh)))
				{
					if (res == TOP_E_CONN) {
						return_error(e_text, e_size, TOP_E_CONN,
							" %s %s", name_buf, name_buf_neigh);
						return TOP_E_CONN;
					} else {
						return return_error(e_text, e_size, res, "");
					}
				}
			}

			for (int k = 0; k < g_b->nodes[j].n_adj; k++) {
				if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_GATE)
					continue;
				name_len = strlen(name_buf) +
					strlen(g_b->nodes[g_b->nodes[j].adj[k].n].name) + 2;
				if (name_buf_neigh_cap < name_len) {
					name_buf_neigh_cap = (1 + name_len / name_buf_blk) *
						name_buf_blk;
					name_buf_neigh = realloc(name_buf_neigh,
						name_buf_neigh_cap);
					if (!name_buf_neigh) {
						return return_error(e_text, e_size,
							TOP_E_ALLOC, "");
					}
				}
				sprintf(name_buf_neigh, "%s.%s", name_buf,
					g_b->nodes[g_b->nodes[j].adj[k].n].name);
				if (graph_add_node(g_prod, name_buf_neigh, NODE_GATE))
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh)))
				{
					if (res == TOP_E_CONN) {
						return_error(e_text, e_size, TOP_E_CONN,
							" %s %s", name_buf, name_buf_neigh);
						return TOP_E_CONN;
					} else {
						return return_error(e_text, e_size, res, "");
					}
				}
			}
		}
	}

	for (int i = 0; i < g_a->n_nodes; i++) {
		if (g_a->nodes[i].type != NODE_NODE) continue;
		for (int j = 0; j < g_b->n_nodes; j++) {
			if (g_b->nodes[j].type != NODE_NODE) continue;

			sprintf(name_buf, "(%s,%s)", g_a->nodes[i].name,
				g_b->nodes[j].name);

			for (int k = 0; k < g_a->nodes[i].n_adj; k++) {
				if (g_a->nodes[g_a->nodes[i].adj[k].n].type != NODE_NODE)
					continue;
				if (g_a->nodes[i].adj[k].n < i) continue;

				name_len =
					strlen(g_a->nodes[g_a->nodes[i].adj[k].n].name) +
					strlen(g_b->nodes[j].name) + 4;
				if (name_buf_neigh_cap < name_len) {
					name_buf_neigh_cap = (1 + name_len / name_buf_blk) *
						name_buf_blk;
					name_buf_neigh = realloc(name_buf_neigh,
						name_buf_neigh_cap);
					if (!name_buf_neigh) {
						return return_error(e_text, e_size,
							TOP_E_ALLOC, "");
					}
				}
				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[g_a->nodes[i].adj[k].n].name,
					g_b->nodes[j].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh)))
				{
					if (res == TOP_E_CONN) {
						return_error(e_text, e_size, TOP_E_CONN,
							" %s %s", name_buf, name_buf_neigh);
						return TOP_E_CONN;
					} else {
						return return_error(e_text, e_size, res, "");
					}
				}
			}

			for (int k = 0; k < g_b->nodes[j].n_adj; k++) {
				if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_NODE)
					continue;
				if (g_b->nodes[j].adj[k].n < j) continue;

				name_len = strlen(g_a->nodes[i].name) +
					strlen(g_b->nodes[g_b->nodes[j].adj[k].n].name) + 4;
				if (name_buf_neigh_cap < name_len) {
					name_buf_neigh_cap = (1 + name_len / name_buf_blk) *
						name_buf_blk;
					name_buf_neigh = realloc(name_buf_neigh,
						name_buf_neigh_cap);
					if (!name_buf_neigh) {
						return return_error(e_text, e_size,
							TOP_E_ALLOC, "");
					}
				}
				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[i].name,
					g_b->nodes[g_b->nodes[j].adj[k].n].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh)))
				{
					if (res == TOP_E_CONN) {
						return_error(e_text, e_size, TOP_E_CONN,
							" %s %s", name_buf, name_buf_neigh);
						return TOP_E_CONN;
					} else {
						return return_error(e_text, e_size, res, "");
					}
				}
			}
		}
	}

	free(name_buf);
	free(name_buf_neigh);
	return 0;
}

static int
graph_insert (graph_t *g, graph_t *g_prod, name_stack_t *s,
	char *e_text, size_t e_size)
{
	(void) s;
	int res;
	char *stack_name = name_stack_name(s);
	if (!stack_name) return return_error(e_text, e_size, TOP_E_ALLOC, "");

	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

	for (int i = 0; i < g_prod->n_nodes; i++) {
		int name_len = strlen(g_prod->nodes[i].name) +
			strlen(stack_name) + 2;
		if (name_buf_cap < name_len) {
			name_buf_cap = (1 + name_len / name_buf_blk) *
				name_buf_blk;
			name_buf = realloc(name_buf, name_buf_cap);
			if (!name_buf) {
				return return_error(e_text, e_size,
					TOP_E_ALLOC, "");
			}
		}
		sprintf(name_buf, "%s.%s", stack_name, g_prod->nodes[i].name);

		if (graph_add_node(g, name_buf,
			g_prod->nodes[i].type))
		{
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
	}

	char *name_buf_2 = malloc(name_buf_cap);
	if (!name_buf_2)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	for (int i = 0; i < g_prod->n_nodes; i++) {
		for (int j = 0; j < g_prod->nodes[i].n_adj; j++) {
			if (i < g_prod->nodes[i].adj[j].n) continue;
			sprintf(name_buf, "%s.%s", stack_name, g_prod->nodes[i].name);
			sprintf(name_buf_2, "%s.%s", stack_name,
				g_prod->nodes[g_prod->nodes[i].adj[j].n].name);
			if ((res = graph_add_edge_name(g, name_buf, name_buf_2))) {
				if (res == TOP_E_CONN) {
					return_error(e_text, e_size, TOP_E_CONN,
						" %s %s", name_buf, name_buf_2);
					return TOP_E_CONN;
				} else {
					return return_error(e_text, e_size, res, "");
				}
			}
		}
	}
	free(stack_name);
	free(name_buf);
	free(name_buf_2);
	return 0;
}

static int
add_submodule (submodule_wrapper_t *smodule,
	network_definition_t *net, graph_t *g,
	name_stack_t *s, param_stack_t *p,
	char *e_text, size_t e_size)
{
	int res;
	if (smodule->type == SUBM_HAS_PROD) {
		graph_t *g_a = graph_create();
		if (!g_a) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		graph_t *g_b = graph_create();
		if (!g_b) {
			topologies_graph_destroy(g_a);
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}

		name_stack_t *s_tmp = name_stack_create("");
		if ((res = add_submodule(smodule->ptr.prod->a,
			net, g_a, s_tmp, p,
			e_text, e_size)))
		{
			return res;
		}
		topologies_graph_compact((void **) &g_a, e_text, e_size);

		if ((res = add_submodule(smodule->ptr.prod->b,
			net, g_b, s_tmp, p,
			e_text, e_size)))
		{
			return res;
		}
		topologies_graph_compact((void **) &g_b, e_text, e_size);
		free(s_tmp->name);
		free(s_tmp);
		graph_t *g_prod = graph_create();
		if (!g_b) {
			topologies_graph_destroy(g_a);
			topologies_graph_destroy(g_b);
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		if ((res = graphs_product(g_a, g_b, g_prod, e_text, e_size))) {
			return res;
		}
		topologies_graph_destroy(g_a);
		topologies_graph_destroy(g_b);
		if ((res = graph_insert(g, g_prod, s, e_text, e_size))) {
			return res;
		}
		topologies_graph_destroy(g_prod);
	} else if (smodule->type == SUBM_HAS_SUBM) {
		submodule_plain_t *sm = smodule->ptr.subm;
		for (int i = 0; i < sm->n_params; i++) {
			if ((res = param_stack_enter(p, &sm->params[i], e_text,
				e_size)))
			{
				return res;
			}
		}
		int size;
		double size_d;
		if (sm->size == NULL) {
			size = 0;
		} else {
			if ((res = param_stack_eval(p, sm->size, &size_d, e_text,
				e_size)))
			{
				return res;
			}
			size = lrint(size_d);
		}
		if (size > 0) {
			for (int j = 0; j < size; j++) {
				if ((res = enter_and_expand_module(net, g, s, p,
					sm, j, e_text, e_size)))
				{
					return res;
				}
			}
		} else {
			if ((res = enter_and_expand_module(net, g, s, p, sm, -1,
				e_text, e_size)))
			{
				return res;
			}
		}
		for (int i = 0; i < sm->n_params; i++) {
			param_stack_leave(p);
		}
	} else { /* SUBM_HAD_COND */
		submodule_cond_t *sc = smodule->ptr.cond;
		double tmp_d;
		if ((res = param_stack_eval(p, sc->condition, &tmp_d,
			e_text, e_size)))
		{
			return res;
		}
		int condition = lrint(tmp_d);
		if (condition) {
			if ((res = add_submodule(sc->subm_then, net, g, s, p,
				e_text, e_size)))
			{
				return res;
			}
		} else if (sc->subm_else) {
			if ((res = add_submodule(sc->subm_else, net, g, s, p,
				e_text, e_size)))
			{
				return res;
			}
		}
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
	if (module->type == MODULE_SIMPLE) {
		/* add gates and connect them to the node */
		char *name_s = name_stack_name(s);
		if (!name_s)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		if (graph_add_node(g, name_s, NODE_NODE)) {
			free(name_s);
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		char *node_attrs = malloc(strlen(module->attributes) + 1);
		if (!node_attrs)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		strncpy(node_attrs, module->attributes, strlen(module->attributes) + 1);
		g->nodes[graph_find_node(g, name_s)].attributes = node_attrs;
		for (int i = 0; i < module->n_gates; i++) {
			double size_d;
			if ((res = param_stack_eval(p, module->gates[i].size,
				&size_d, e_text, e_size)))
			{
				return res;
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
			if ((res = param_stack_eval(p, module->gates[i].size,
				&size_d, e_text, e_size)))
			{
				return res;
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
			submodule_wrapper_t *smodule = &module->submodules[i];
			if ((res = add_submodule(smodule, net, g, s, p,
				e_text, e_size)))
			{
				return res;
			}
		}
		for (int i = 0; i < module->n_connections; i++) {
			if ((res = traverse_and_add_conns(&module->connections[i],
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

	g = graph_create();
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
			param_stack_destroy(p);
			free(s->name);
			free(s);
			return res;
		}
	}
	if ((res = expand_module(g, root_module, net, s, p, e_text, e_size)))
	{
		param_stack_destroy(p);
		topologies_graph_destroy(g);
		free(s->name);
		free(s);
		return res;
	}

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
graph_find_end_and_mark (graph_t *g, int prev, int n, int *n_node_res,
	char *e_text, size_t e_size)
{
	node_t *node_tmp;
	node_t *to = &(g->nodes[n]);

	node_tmp = to;
	while (node_tmp != NULL) {
		if ((node_tmp->type != NODE_NODE) &&
			(node_tmp->n_adj > 2))
		{
			return return_error(e_text,
				e_size, TOP_E_BADGATE, ": %s", node_tmp->name);
		}
		if (g->nodes[node_tmp->adj[0].n].type == NODE_GATE) {
			node_tmp->type = NODE_GATE_VISITED;
			node_tmp = &(g->nodes[node_tmp->adj[0].n]);
		} else if ((g->nodes[node_tmp->adj[0].n].type == NODE_NODE) &&
			(node_tmp->adj[0].n != prev))
		{
			node_tmp->type = NODE_GATE_VISITED;
			node_tmp = &(g->nodes[node_tmp->adj[0].n]);
			break;
		} else {
			if (node_tmp->n_adj == 1) {
				break;
			} else if (g->nodes[node_tmp->adj[1].n].type == NODE_GATE) {
				node_tmp->type = NODE_GATE_VISITED;
				node_tmp = &(g->nodes[node_tmp->adj[1].n]);
			} else {
				node_tmp->type = NODE_GATE_VISITED;
				node_tmp = &(g->nodes[node_tmp->adj[1].n]);
				break;
			}
		}
	}

	*n_node_res = node_tmp->n;
	return 0;
}

int
topologies_graph_compact (void **v, char *e_text, size_t e_size)
{
	int res;
	int n_node_a, n_node_b;
	graph_t *g = (graph_t *) *v;
	graph_t *new_g = graph_create();
	if (new_g == NULL)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type != NODE_NODE)
			continue;

		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if (g->nodes[g->nodes[i].adj[j].n].type == NODE_GATE) {
				res = graph_find_end_and_mark(g, i,
					g->nodes[i].adj[j].n, &n_node_a, e_text, e_size);
				if (res) {
					topologies_graph_destroy(new_g);
					return res;
				}
				if (!graph_are_adjacent(&g->nodes[n_node_a], &g->nodes[i])) {
					if ((res = graph_add_edge_id(g, n_node_a,
						i)))
					{
						topologies_graph_destroy(new_g);
						return return_error(e_text,
							e_size, res, "");
					}
				}
			}
		}
	}

	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type == NODE_GATE_VISITED)
			continue;
		if (g->nodes[i].n_adj == 0)
			continue;
		if (graph_add_node(new_g, g->nodes[i].name, g->nodes[i].type))
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	for (int i = 0; i < g->n_nodes; i++) {
		if (g->nodes[i].type == NODE_GATE_VISITED)
			continue;
		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if ((i < g->nodes[i].adj[j].n) &&
				(g->nodes[g->nodes[i].adj[j].n].type !=
				NODE_GATE_VISITED))
			{
				n_node_a = graph_find_node(new_g,
					g->nodes[i].name);
				n_node_b = graph_find_node(new_g,
					g->nodes[g->nodes[i].adj[j].n].name);
				graph_add_edge_id(new_g, n_node_a, n_node_b);
			}
		}
	}
	topologies_graph_destroy(g);
	*v = (void *) new_g;

	return 0;
}

static void
free_submodule (submodule_wrapper_t *s)
{
	if (s->type == SUBM_HAS_SUBM) {
		free(s->ptr.subm->name);
		free(s->ptr.subm->module);
		free(s->ptr.subm->size);
		if (s->ptr.subm->n_params) {
			for (int k = 0; k < s->ptr.subm->n_params; k++)
			{
				free(s->ptr.subm->params[k].name);
				free(s->ptr.subm->params[k].value);
			}
			free(s->ptr.subm->params);
		}
		free(s->ptr.subm);
	} else if (s->type == SUBM_HAS_COND) {
		free_submodule(s->ptr.cond->subm_then);
		free(s->ptr.cond->subm_then);
		if (s->ptr.cond->subm_else) {
			free_submodule(s->ptr.cond->subm_else);
			free(s->ptr.cond->subm_else);
		}
		free(s->ptr.cond->condition);
		free(s->ptr.cond);
	} else {
		free_submodule(s->ptr.prod->a);
		free(s->ptr.prod->a);
		free_submodule(s->ptr.prod->b);
		free(s->ptr.prod->b);
		free(s->ptr.prod);
	}
}

static void
free_connection (connection_wrapper_t *c)
{
	if (c->type == CONN_HAS_CONN) {
		free(c->ptr.conn->from);
		free(c->ptr.conn->to);
		free(c->ptr.conn);
	} else if (c->type == CONN_HAS_COND) {
		free_connection(c->ptr.cond->conn_then);
		free(c->ptr.cond->conn_then);
		if (c->ptr.cond->conn_else) {
			free_connection(c->ptr.cond->conn_else);
			free(c->ptr.cond->conn_else);
		}
		free(c->ptr.cond->condition);
		free(c->ptr.cond);
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
			free(n->modules[i].attributes);
			if (n->modules[i].submodules) {
				for (int j = 0; j < n->modules[i].n_submodules; j++) {
					free_submodule(&n->modules[i].submodules[j]);
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
			free(n->network->params);
		}
		free(n->network->module);
		free(n->network);
	}
	free(n);
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

int
topologies_network_read_string (void *net, char *addr, char *e_text, size_t e_size)
{
	return json_read_file(addr, strlen(addr), net, e_text, e_size);
}
