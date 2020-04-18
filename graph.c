#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "graph.h"
#include "topologies.h"
#include "defs.h"
#include "errors.h"

graph_t *
topologies_graph_create (void)
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
graph_add_node (graph_t *g, char *name, node_type type)
{
	int i = g->n_nodes;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = (node_t *) realloc(g->nodes,
			g->cap_nodes * sizeof(node_t));
		if (!g->nodes)
			return TOP_E_ALLOC;
	}
	g->nodes[i].name = (char *) malloc(strlen(name) + 1);
	if (!g->nodes[i].name)
		return TOP_E_ALLOC;
	strncpy(g->nodes[i].name, name, strlen(name) + 1);
	g->nodes[i].adj = (int *) malloc(ADJ_BLK_SIZE * sizeof(int));
	if (!g->nodes[i].adj)
		return TOP_E_ALLOC;
	g->nodes[i].n_adj = 0;
	g->nodes[i].cap_adj = ADJ_BLK_SIZE;
	g->nodes[i].n = i;
	g->nodes[i].type = type;
	g->n_nodes++;
	return 0;
}

node_t *
graph_find_node (graph_t *g, char *name)
{
	for (int i = 0; i < g->n_nodes; i++)
		if (strcmp(g->nodes[i].name, name) == 0)
			return &(g->nodes[i]);
	return NULL;
}

int
graph_add_edge_ptr (node_t *node_a, node_t *node_b)
{
	if ((node_a == NULL) || (node_b == NULL))
		return TOP_E_CONN;

	int i = node_a->n_adj;
	if (node_a->n_adj == node_a->cap_adj) {
		node_a->cap_adj += ADJ_BLK_SIZE;
		node_a->adj = (int *) realloc(node_a->adj,
			node_a->cap_adj * sizeof(int));
		if (!node_a->adj)
			return TOP_E_ALLOC;
	}
	node_a->adj[i] = node_b->n;
	node_a->n_adj++;

	i = node_b->n_adj;
	if (node_b->n_adj == node_b->cap_adj) {
		node_b->cap_adj += ADJ_BLK_SIZE;
		node_b->adj = (int *) realloc(node_b->adj,
			node_b->cap_adj * sizeof(int));
		if (!node_b->adj)
			return TOP_E_ALLOC;
	}
	node_b->adj[i] = node_a->n;
	node_b->n_adj++;

	return 0;
}

bool
graph_are_adjacent (node_t *node_a, node_t *node_b)
{
	if (!node_a || !node_b) return false;
	for (int j = 0; j < node_a->n_adj; j++) {
		if (node_a->adj[j] == node_b->n)
			return true;
	}
	return false;
}

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b)
{
	node_t *node_a, *node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
	if (node_a == NULL)
		return TOP_E_CONN;
	if (node_b == NULL)
		return TOP_E_CONN;
	return graph_add_edge_ptr(node_a, node_b);
}

void
topologies_graph_print (graph_t *g, FILE *stream, bool print_gate_nodes)
{
	fprintf(stream, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type == NODE_NODE) || print_gate_nodes)
			fprintf(stream, "n%d [label=\"%s\"];\n",
				i, g->nodes[i].name);
	}
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type != NODE_NODE) && !print_gate_nodes)
			continue;
		for (int j = 0; j < g->nodes[i].n_adj; j++) {
			if (i > g->nodes[i].adj[j]) continue;
			if (!print_gate_nodes &&
				g->nodes[g->nodes[i].adj[j]].type != NODE_NODE)
			{
				continue;
			}
			fprintf(stream, "n%d -- n%d;\n", i, g->nodes[i].adj[j]);
		}
	}
	fprintf(stream, "}\n");
}

void
topologies_graph_destroy (graph_t *g)
{
	for (int i = 0; i < g->n_nodes; i++) {
		free(g->nodes[i].name);
		free(g->nodes[i].adj);
	}
	free(g->nodes);
	free(g);
}

