#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <regex.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "parser.h"
#include "name_stack.h"
#include "param_stack.h"
#include "graph.h"
#include "topologies.h"
#include "products.h"
#include "errors.h"

static int
graphs_cart_product_nodes (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	int *r_name_buf_cap, int *r_name_buf_neigh_cap,
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
			char *attrs;
			if (g_a->nodes[i].attributes &&
				g_b->nodes[j].attributes)
			{
				int attrs_len = strlen(g_a->nodes[i].attributes) +
					strlen(g_b->nodes[j].attributes) + 3;
				attrs = malloc(attrs_len);
				if (!attrs)
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				sprintf(attrs, "%s, %s",
					g_a->nodes[i].attributes,
					g_b->nodes[j].attributes);
			} else if (g_a->nodes[i].attributes) {
				attrs = g_a->nodes[i].attributes;
			} else {
				attrs = g_b->nodes[j].attributes;
			}
			if (graph_add_node(g_prod, name_buf, NODE_NODE, attrs))
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			if (g_a->nodes[i].attributes && g_b->nodes[j].attributes)
				free(attrs);

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
				if (graph_add_node(g_prod, name_buf_neigh, NODE_GATE, NULL))
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_a->nodes[i].adj[k].attributes)))
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
				if (graph_add_node(g_prod, name_buf_neigh, NODE_GATE, NULL))
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_b->nodes[j].adj[k].attributes)))
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

	*r_name_buf_cap = name_buf_cap;
	*r_name_buf_neigh_cap = name_buf_neigh_cap;

	free(name_buf);
	free(name_buf_neigh);
	return 0;
}

int
graphs_cart_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size)
{
	int res;
	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	int name_buf_neigh_cap = name_buf_blk;

	graphs_cart_product_nodes(g_a, g_b, g_prod, &name_buf_cap,
		&name_buf_neigh_cap, e_text, e_size);

	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (name_buf_neigh_cap < name_buf_cap)
		name_buf_neigh_cap = name_buf_cap;
	char *name_buf_neigh = malloc(name_buf_neigh_cap);
	if (!name_buf_neigh)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

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

				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[g_a->nodes[i].adj[k].n].name,
					g_b->nodes[j].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_a->nodes[i].adj[k].attributes)))
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

				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[i].name,
					g_b->nodes[g_b->nodes[j].adj[k].n].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_b->nodes[j].adj[k].attributes)))
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

int
graphs_tens_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size)
{
	int res;
	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	int name_buf_neigh_cap = name_buf_blk;

	graphs_cart_product_nodes(g_a, g_b, g_prod, &name_buf_cap,
		&name_buf_neigh_cap, e_text, e_size);

	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (name_buf_neigh_cap < name_buf_cap)
		name_buf_neigh_cap = name_buf_cap;
	char *name_buf_neigh = malloc(name_buf_neigh_cap);
	if (!name_buf_neigh)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

	for (int i = 0; i < g_a->n_nodes; i++) {
		if (g_a->nodes[i].type != NODE_NODE) continue;
		for (int j = 0; j < g_b->n_nodes; j++) {
			if (g_b->nodes[j].type != NODE_NODE) continue;

			sprintf(name_buf, "(%s,%s)", g_a->nodes[i].name,
				g_b->nodes[j].name);

			for (int k = 0; k < g_a->nodes[i].n_adj; k++) {
				if (g_a->nodes[g_a->nodes[i].adj[k].n].type != NODE_NODE) continue;
				for (int l = 0; l < g_b->nodes[j].n_adj; l++) {
					if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_NODE) continue;

					sprintf(name_buf_neigh, "(%s,%s)",
						g_a->nodes[g_a->nodes[i].adj[k].n].name,
						g_b->nodes[g_b->nodes[j].adj[l].n].name);

					if ((res = graph_add_edge_name(g_prod, name_buf,
						name_buf_neigh,
						g_b->nodes[j].adj[k].attributes)))
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
	}

	free(name_buf);
	free(name_buf_neigh);
	return 0;
}

int
graphs_lex_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size)
{
	int res;
	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	int name_buf_neigh_cap = name_buf_blk;

	graphs_cart_product_nodes(g_a, g_b, g_prod, &name_buf_cap,
		&name_buf_neigh_cap, e_text, e_size);

	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (name_buf_neigh_cap < name_buf_cap)
		name_buf_neigh_cap = name_buf_cap;
	char *name_buf_neigh = malloc(name_buf_neigh_cap);
	if (!name_buf_neigh)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

	for (int i = 0; i < g_a->n_nodes; i++) {
		if (g_a->nodes[i].type != NODE_NODE) continue;
		for (int j = 0; j < g_b->n_nodes; j++) {
			if (g_b->nodes[j].type != NODE_NODE) continue;

			sprintf(name_buf, "(%s,%s)", g_a->nodes[i].name,
				g_b->nodes[j].name);

			for (int k = 0; k < g_a->nodes[i].n_adj; k++) {
				if (g_a->nodes[g_a->nodes[i].adj[k].n].type != NODE_NODE) continue;

				for (int l = 0; l < g_b->n_nodes; l++) {
					if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_NODE) continue;

					sprintf(name_buf_neigh, "(%s,%s)",
						g_a->nodes[g_a->nodes[i].adj[k].n].name,
						g_b->nodes[l].name);

					if ((res = graph_add_edge_name(g_prod, name_buf,
						name_buf_neigh,
						g_b->nodes[j].adj[k].attributes)))
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

			for (int k = 0; k < g_b->nodes[j].n_adj; k++) {
				if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_NODE) continue;

				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[i].name,
					g_b->nodes[g_b->nodes[j].adj[k].n].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_b->nodes[j].adj[k].attributes)))
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

int
graphs_strong_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size)
{
	int res;
	int name_buf_blk = 32;
	int name_buf_cap = name_buf_blk;
	int name_buf_neigh_cap = name_buf_blk;

	graphs_cart_product_nodes(g_a, g_b, g_prod, &name_buf_cap,
		&name_buf_neigh_cap, e_text, e_size);

	char *name_buf = malloc(name_buf_cap);
	if (!name_buf)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	if (name_buf_neigh_cap < name_buf_cap)
		name_buf_neigh_cap = name_buf_cap;
	char *name_buf_neigh = malloc(name_buf_neigh_cap);
	if (!name_buf_neigh)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");

	for (int i = 0; i < g_a->n_nodes; i++) {
		if (g_a->nodes[i].type != NODE_NODE) continue;
		for (int j = 0; j < g_b->n_nodes; j++) {
			if (g_b->nodes[j].type != NODE_NODE) continue;

			sprintf(name_buf, "(%s,%s)", g_a->nodes[i].name,
				g_b->nodes[j].name);

			for (int k = 0; k < g_a->nodes[i].n_adj; k++) {
				if (g_a->nodes[g_a->nodes[i].adj[k].n].type != NODE_NODE) continue;
				for (int l = 0; l < g_b->nodes[j].n_adj; l++) {
					if (g_b->nodes[g_b->nodes[j].adj[k].n].type != NODE_NODE) continue;

					sprintf(name_buf_neigh, "(%s,%s)",
						g_a->nodes[g_a->nodes[i].adj[k].n].name,
						g_b->nodes[g_b->nodes[j].adj[l].n].name);

					if ((res = graph_add_edge_name(g_prod, name_buf,
						name_buf_neigh,
						g_b->nodes[j].adj[k].attributes)))
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

				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[g_a->nodes[i].adj[k].n].name,
					g_b->nodes[j].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_a->nodes[i].adj[k].attributes)))
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

				sprintf(name_buf_neigh, "(%s,%s)",
					g_a->nodes[i].name,
					g_b->nodes[g_b->nodes[j].adj[k].n].name);

				if ((res = graph_add_edge_name(g_prod, name_buf,
					name_buf_neigh,
					g_b->nodes[j].adj[k].attributes)))
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
