#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "graph.h"
#include "topologies.h"
#include "defs.h"
#include "errors.h"

graph_t *
graph_create (void)
{
	graph_t *g = (graph_t *) malloc(sizeof(graph_t));
	if (!g)
		return NULL;
	g->n_nodes = 0;
	g->cap_nodes = GRAPH_BLK_SIZE;
	g->nodes = (node_t *) calloc(g->cap_nodes, sizeof(node_t));
	if (!g->nodes) {
		free(g);
		return NULL;
	}
	return g;
}

int
graph_add_node (graph_t *g, char *name, node_type type, char *attrs)
{
	int i = g->n_nodes;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = (node_t *) realloc(g->nodes,
			g->cap_nodes * sizeof(node_t));
		if (!g->nodes)
			return TOP_E_ALLOC;
		memset(g->nodes + (g->cap_nodes - GRAPH_BLK_SIZE), 0,
			GRAPH_BLK_SIZE * sizeof(node_t));
	}
	g->nodes[i].name = (char *) malloc(strlen(name) + 1);
	if (!g->nodes[i].name)
		return TOP_E_ALLOC;
	strncpy(g->nodes[i].name, name, strlen(name) + 1);
	g->nodes[i].adj = (edge_t *) malloc(ADJ_BLK_SIZE * sizeof(edge_t));
	memset(g->nodes[i].adj, 0, ADJ_BLK_SIZE * sizeof(edge_t));
	if (!g->nodes[i].adj)
		return TOP_E_ALLOC;
	g->nodes[i].n_adj = 0;
	g->nodes[i].cap_adj = ADJ_BLK_SIZE;
	g->nodes[i].n = i;
	g->nodes[i].type = type;
	if (attrs) {
		char *node_attrs = malloc(strlen(attrs) + 1);
		if (!node_attrs)
			return TOP_E_ALLOC;
		strncpy(node_attrs, attrs, strlen(attrs) + 1);
		g->nodes[i].attributes = node_attrs;
	} else {
		g->nodes[i].attributes = NULL;
	}
	g->n_nodes++;
	return 0;
}

int
graph_find_node (graph_t *g, char *name)
{
	for (int i = 0; i < g->n_nodes; i++)
		if ((strcmp(g->nodes[i].name, name) == 0) &&
			(g->nodes[i].type != NODE_REPLACED) &&
			(g->nodes[i].type != NODE_REPLACED_T))
				return i;
	return -1;
}

int
graph_add_edge_id (graph_t *g, int n_a, int n_b, char *attrs)
{
	if ((n_a < 0) || (n_b < 0) || (n_a == n_b) || (n_a > g->n_nodes) || (n_b > g->n_nodes))
		return TOP_E_CONN;
	for (int i = 0; i < g->nodes[n_a].n_adj; i++)
		if (g->nodes[n_a].adj[i].n == n_b)
			return 0;

	node_t *node_a = &(g->nodes[n_a]);
	node_t *node_b = &(g->nodes[n_b]);
	int i = node_a->n_adj;
	if (node_a->n_adj == node_a->cap_adj) {
		node_a->cap_adj += ADJ_BLK_SIZE;
		node_a->adj = (edge_t *) realloc(node_a->adj,
			node_a->cap_adj * sizeof(edge_t));
		if (!node_a->adj)
			return TOP_E_ALLOC;
		memset(node_a->adj + (node_a->cap_adj - ADJ_BLK_SIZE), 0,
			ADJ_BLK_SIZE * sizeof(edge_t));
	}
	node_a->adj[i].n = node_b->n;
	node_a->adj[i].attributes = NULL;
	if (attrs) {
		node_a->adj[i].attributes = (char *) malloc(strlen(attrs) + 1);
		if (!node_a->adj[i].attributes)
			return TOP_E_ALLOC;
		strncpy(node_a->adj[i].attributes, attrs, strlen(attrs) + 1);
	}
	node_a->n_adj++;

	i = node_b->n_adj;
	if (node_b->n_adj == node_b->cap_adj) {
		node_b->cap_adj += ADJ_BLK_SIZE;
		node_b->adj = (edge_t *) realloc(node_b->adj,
			node_b->cap_adj * sizeof(edge_t));
		if (!node_b->adj)
			return TOP_E_ALLOC;
		memset(node_b->adj + (node_b->cap_adj - ADJ_BLK_SIZE), 0,
			ADJ_BLK_SIZE * sizeof(edge_t));
	}
	node_b->adj[i].n = node_a->n;
	node_b->adj[i].attributes = NULL;
	if (attrs) {
		node_b->adj[i].attributes = (char *) malloc(strlen(attrs) + 1);
		if (!node_b->adj[i].attributes)
			return TOP_E_ALLOC;
		strncpy(node_b->adj[i].attributes, attrs, strlen(attrs) + 1);
	}
	node_b->n_adj++;

	return 0;
}

bool
graph_are_adjacent (node_t *node_a, node_t *node_b)
{
	if (!node_a || !node_b) return false;
	for (int j = 0; j < node_a->n_adj; j++) {
		if (node_a->adj[j].n == node_b->n)
			return true;
	}
	return false;
}

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b, char *attrs)
{
	int node_a, node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
	if (node_a < 0)
		return TOP_E_CONN;
	if (node_b < 0)
		return TOP_E_CONN;
	return graph_add_edge_id(g, node_a, node_b, attrs);
}

void
topologies_graph_print (graph_t *g, FILE *stream, bool print_gate_nodes)
{
	fprintf(stream, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type == NODE_NODE) || print_gate_nodes) {
			fprintf(stream, "n%d [label=\"%s\"",
				i, g->nodes[i].name);
			if (g->nodes[i].attributes)
				fprintf(stream, ", %s", g->nodes[i].attributes);
			fprintf(stream, "];\n");
		}
	}
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type != NODE_NODE) && !print_gate_nodes)
			continue;
		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if (i > g->nodes[i].adj[j].n) continue;
			if (!print_gate_nodes &&
				g->nodes[g->nodes[i].adj[j].n].type != NODE_NODE)
			{
				continue;
			}
			fprintf(stream, "n%d -- n%d", i, g->nodes[i].adj[j].n);
			if (g->nodes[i].adj[j].attributes)
				fprintf(stream, " [%s]", g->nodes[i].adj[j].attributes);
			fprintf(stream, ";\n");
		}
	}
	fprintf(stream, "}\n");
}

char *
topologies_graph_string (graph_t *g, bool print_gate_nodes)
{
	int buf_len = 0;

	buf_len += snprintf(0, 0, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type == NODE_NODE) || print_gate_nodes) {
			buf_len += snprintf(0, 0, "n%d [label=\"%s\"",
				i, g->nodes[i].name);
			if (g->nodes[i].attributes)
				buf_len += snprintf(0, 0, ", %s", g->nodes[i].attributes);
			buf_len += snprintf(0, 0, "];\n");
		}
	}
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type != NODE_NODE) && !print_gate_nodes)
			continue;
		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if (i > g->nodes[i].adj[j].n) continue;
			if (!print_gate_nodes &&
				g->nodes[g->nodes[i].adj[j].n].type != NODE_NODE)
			{
				continue;
			}
			buf_len += snprintf(0, 0, "n%d -- n%d", i, g->nodes[i].adj[j].n);
			if (g->nodes[i].adj[j].attributes)
				buf_len += snprintf(0, 0, " [%s]", g->nodes[i].adj[j].attributes);
			buf_len += snprintf(0, 0, ";\n");
		}
	}
	buf_len += snprintf(0, 0, "}\n");

	char *buf = malloc(buf_len + 1);
	if (!buf) return NULL;

	buf_len = 0;
	buf_len += sprintf(buf, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type == NODE_NODE) || print_gate_nodes) {
			buf_len += sprintf(buf + buf_len, "n%d [label=\"%s\"",
				i, g->nodes[i].name);
			if (g->nodes[i].attributes) {
				buf_len += sprintf(buf + buf_len, ", %s",
					g->nodes[i].attributes);
			}
			buf_len += sprintf(buf + buf_len, "];\n");
		}
	}
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type != NODE_NODE) && !print_gate_nodes)
			continue;
		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if (i > g->nodes[i].adj[j].n) continue;
			if (!print_gate_nodes &&
				g->nodes[g->nodes[i].adj[j].n].type != NODE_NODE)
			{
				continue;
			}
			buf_len += sprintf(buf + buf_len, "n%d -- n%d", i, g->nodes[i].adj[j].n);
			if (g->nodes[i].adj[j].attributes) {
				buf_len += sprintf(buf + buf_len, " [%s]",
					g->nodes[i].adj[j].attributes);
			}
			buf_len += sprintf(buf + buf_len, ";\n");
		}
	}
	buf_len += sprintf(buf + buf_len, "}\n");
	return buf;
}

void
topologies_graph_string_free (char *string)
{
	free(string);
}

void
topologies_graph_destroy (graph_t *g)
{
	if (g->nodes) {
		for (int i = 0; i < g->n_nodes; i++) {
			free(g->nodes[i].name);
			for (int j = 0; j < g->nodes[i].n_adj; j++) {
				if (g->nodes[i].adj[j].attributes)
					free(g->nodes[i].adj[j].attributes);
			}
			free(g->nodes[i].adj);
			if (g->nodes[i].attributes)
				free(g->nodes[i].attributes);
		}
		free(g->nodes);
	}
	free(g);
}

