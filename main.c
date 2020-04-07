#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "parser.h"
#include "defs.h"

void
error (const char * errmsg, ...)
{
	va_list args;
	va_start(args, errmsg);
	fprintf(stderr, errmsg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

off_t
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


graph_t *
graph_create ()
{
	graph_t *g = malloc(sizeof(graph_t));
	g->n_nodes = 0;
	g->cap_nodes = GRAPH_BLK_SIZE;
	g->nodes = calloc(g->cap_nodes, sizeof(node_t));
	return g;
}

node_t *
graph_add_node (graph_t *g, char *name)
{
	int i = g->n_nodes;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = realloc(g->nodes, g->cap_nodes * sizeof(node_t));
	}
	g->nodes[i].name = malloc(strlen(name) + 1);
	strncpy(g->nodes[i].name, name, strlen(name) + 1);
	g->nodes[i].adj = NULL;
	g->nodes[i].n = i;
	g->n_nodes++;
	return &(g->nodes[i]);
}

node_t *
graph_find_node (graph_t *g, char *name)
{
	for (int i = 0; i < g->n_nodes; i++)
		if (strcmp(g->nodes[i].name, name) == 0)
			return &(g->nodes[i]);
	return NULL;
}

void
graph_add_edge (graph_t *g, char *name_a, char *name_b)
{
	node_t *node_a, *node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
	if (node_a == NULL)
		node_a = graph_add_node(g, name_a);
	if (node_b == NULL)
		node_b = graph_add_node(g, name_b);
	node_list_t *l = malloc(sizeof(node_list_t));
	l->next = node_a->adj;
	l->node = node_b;
	node_a->adj = l;
	l = malloc(sizeof(node_list_t));
	l->next = node_b->adj;
	l->node = node_a;
	node_b->adj = l;
}

void
graph_print (graph_t *g, FILE *stream)
{
	node_list_t *l, *l_next;
	fprintf(stream, "graph g {\n");
	for (int i = 0; i < g->n_nodes; i++) {
		fprintf(stream, "n%d [label=\"%s\"];\n", i, g->nodes[i].name);
	}
	for (int i = 0; i < g->n_nodes; i++) {
		l = g->nodes[i].adj;
		while (l != NULL) {
			l_next = l->next;
				if (i < l->node->n)
					fprintf(stream, "n%d -- n%d;\n",
					        i, l->node->n);
			l = l_next;
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


stack_t *
stack_create (char *name)
{
	stack_t *s = malloc(sizeof(stack_t));
	s->next = NULL;
	s->name = malloc(strlen(name) + 1);
	strncpy(s->name, name, strlen(name) + 1);
	return s;
}

void
stack_enter (stack_t *s, char *name, int index)
{
	int len;
	stack_t *head = s;
	while (head->next != NULL)
		head = head->next;
	head->next = malloc(sizeof(stack_t));
	head = head->next;
	head->next = NULL;
	if (index < 0) {
		len = strlen(name) + 1;
		head->name = malloc(len);
		strncpy(s->name, name, len);
	} else {
		len = snprintf(0, 0, "%s[%d]", name, index) + 1;
		head->name = malloc(len);
		snprintf(head->name, len, "%s[%d]", name, index);
	}
}

void
stack_leave (stack_t *s)
{
	stack_t *head = s;
	if (head->next == NULL)
		return;
	while (head->next->next != NULL)
		head = head->next;
	free(head->next->name);
	free(head->next);
	head->next = NULL;
}

char *
stack_name (stack_t *s)
{
	unsigned len = 0, off = 0;
	stack_t *head = s;
	while (head->next != NULL) {
		len += strlen(head->name) + 1;
		head = head->next;
	}
	len += strlen(head->name) + 1;
	char *name = malloc(len);
	while (head->next != NULL) {
		snprintf(name + off, strlen(head->name) + 2, "%s.", head->name);
		off += strlen(head->name) + 1;
		head = head->next;
	}
	snprintf(name + off, strlen(head->name) + 2, "%s.", head->name);
	return name;
}

module_t *
find_module (network_definition_t *net, char *name)
{
	for (int i = 0; i < net->n_modules; i++)
		if (strcmp(net->modules[i].name, name) == 0)
			return &net->modules[i];
	return NULL;
}

void
expand_module (graph_t *g, module_t *module, stack_t *stack)
{
	if (module->submodules == NULL) {
		graph_add_node(g, stack_name(stack));
		return;
	}

}

graph_t *
definition_to_graph (network_definition_t *net)
{
	module_t *root_module = find_module(net, net->network->module);
	graph_t *g = graph_create();
	stack_t *stack = stack_create("network");
	expand_module(g, root_module, stack);
	return g;
}


int
main (int argc, char *argv[])
{
	char *addr = NULL;
	network_definition_t network_definition = { };

	off_t file_size = read_file(argc, argv, &addr);
	int res = json_read_file(addr, file_size, &network_definition);
	if (res < 0)
		error("%s: invalid JSON data: %d\n", argv[1], res);

	graph_t *gg = definition_to_graph(&network_definition);
	graph_print(gg, stdout);
	graph_destroy(gg);

	/* graph_t *g = graph_create();
	graph_add_node(g, "aaa");
	graph_add_node(g, "aab");
	graph_add_node(g, "aac");
	graph_add_node(g, "aad");
	graph_add_node(g, "aae");
	graph_add_edge(g, "aaa", "aab");
	graph_add_edge(g, "aac", "aab");
	graph_add_edge(g, "aad", "aae");
	graph_add_edge(g, "aaa", "aad");
	graph_print(g, stdout);
	graph_destroy(g); */

	exit(EXIT_SUCCESS);
}
