#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "graph.h"
#include "defs.h"

graph_t *
graph_create (void)
{
	graph_t *g = (graph_t *) malloc(sizeof(graph_t));
	g->n_nodes = 0;
	g->cap_nodes = GRAPH_BLK_SIZE;
	g->nodes = (node_t *) calloc(g->cap_nodes, sizeof(node_t));
	return g;
}

node_t *
graph_add_node (graph_t *g, char *name, node_type type)
{
	int i = g->n_nodes;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = (node_t *) realloc(g->nodes,
			g->cap_nodes * sizeof(node_t));
	}
	g->nodes[i].name = (char *) malloc(strlen(name) + 1);
	strncpy(g->nodes[i].name, name, strlen(name) + 1);
	g->nodes[i].adj = NULL;
	g->nodes[i].n = i;
	g->nodes[i].type = type;
	g->n_nodes++;
	return &(g->nodes[i]);
}

static node_t *
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
		return -1;
	node_list_t *l = (node_list_t *) malloc(sizeof(node_list_t));
	l->next = node_a->adj;
	l->node = node_b;
	node_a->adj = l;
	l = (node_list_t *) malloc(sizeof(node_list_t));
	l->next = node_b->adj;
	l->node = node_a;
	node_b->adj = l;
	return 0;
}

int
graph_add_edge_name (graph_t *g, char *name_a, char *name_b)
{
	node_t *node_a, *node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
	if (node_a == NULL)
		return -1;
	if (node_b == NULL)
		return -1;
	return graph_add_edge_ptr(node_a, node_b);
}

void
graph_print (graph_t *g, FILE *stream, bool print_gate_nodes)
{
	node_list_t *l;
	fprintf(stream, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		if ((g->nodes[i].type == NODE_NODE) || print_gate_nodes)
			fprintf(stream, "n%d [label=\"%s\"];\n",
			        i, g->nodes[i].name);
	}
	for (int i = 0; i < g->n_nodes; i++) {
		l = g->nodes[i].adj;
		while (l != NULL) {
				if (i < l->node->n)
					fprintf(stream, "n%d -- n%d;\n",
					        i, l->node->n);
			l = l->next;
		}
	}
	fprintf(stream, "}\n");
}

void
graph_destroy (graph_t *g)
{
	node_list_t *l, *l_next;
	for (int i = 0; i < g->n_nodes; i++) {
		free(g->nodes[i].name);
		l = g->nodes[i].adj;
		while (l != NULL) {
			l_next = l->next;
			free(l);
			l = l_next;
		}
	}
	free(g->nodes);
	free(g);
}

