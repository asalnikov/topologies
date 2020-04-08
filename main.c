#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "tinyexpr.h"

#include "parser.h"
#include "defs.h"

/* file reading */

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

/* graph */

static graph_t *
graph_create (void)
{
	graph_t *g = (graph_t *) malloc(sizeof(graph_t));
	g->n_nodes = 0;
	g->cap_nodes = GRAPH_BLK_SIZE;
	g->nodes = (node_t *) calloc(g->cap_nodes, sizeof(node_t));
	return g;
}

static node_t *
graph_add_node (graph_t *g, char *name, node_type type)
{
	int i = g->n_nodes;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = (node_t *) realloc(g->nodes, g->cap_nodes * sizeof(node_t));
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

static void
graph_add_edge (graph_t *g, char *name_a, char *name_b, node_type type)
{
	node_t *node_a, *node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
	if (node_a == NULL)
		node_a = graph_add_node(g, name_a, type);
	if (node_b == NULL)
		node_b = graph_add_node(g, name_b, type);
	node_list_t *l = (node_list_t *) malloc(sizeof(node_list_t));
	l->next = node_a->adj;
	l->node = node_b;
	node_a->adj = l;
	l = (node_list_t *) malloc(sizeof(node_list_t));
	l->next = node_b->adj;
	l->node = node_a;
	node_b->adj = l;
}

static void
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

static void
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

/* name stack */

static name_stack_t *
name_stack_create (char *name)
{
	name_stack_t *s = (name_stack_t *) malloc(sizeof(name_stack_t));
	s->next = NULL;
	s->name = (char *) malloc(strlen(name) + 1);
	strncpy(s->name, name, strlen(name) + 1);
	return s;
}

static void
name_stack_enter (name_stack_t *s, char *name, int index)
{
	int len;
	name_stack_t *head = s;
	while (head->next != NULL)
		head = head->next;
	head->next = (name_stack_t *) malloc(sizeof(name_stack_t));
	head = head->next;
	head->next = NULL;
	if (index < 0) {
		len = strlen(name) + 1;
		head->name = (char *) malloc(len);
		strncpy(s->name, name, len);
	} else {
		len = snprintf(0, 0, "%s[%d]", name, index) + 1;
		head->name = (char *) malloc(len);
		snprintf(head->name, len, "%s[%d]", name, index);
	}
}

static void
name_stack_leave (name_stack_t *s)
{
	name_stack_t *head = s;
	if (head->next == NULL)
		return;
	while (head->next->next != NULL)
		head = head->next;
	free(head->next->name);
	free(head->next);
	head->next = NULL;
}

static char *
name_stack_name (name_stack_t *s)
{
	unsigned len = 0, off = 0;
	name_stack_t *head = s;
	while (head->next != NULL) {
		len += strlen(head->name) + 1;
		head = head->next;
	}
	len += strlen(head->name) + 1;
	char *name = (char *) malloc(len);
	head = s;
	while (head->next != NULL) {
		snprintf(name + off, strlen(head->name) + 2, "%s.", head->name);
		off += strlen(head->name) + 1;
		head = head->next;
	}
	snprintf(name + off, strlen(head->name) + 2, "%s", head->name);
	return name;
}

static char *
get_full_name (name_stack_t *s, char *name)
{
	char *name_s = name_stack_name(s);
	char *full_name =
		(char *) malloc(strlen(name) + strlen(name_s) + 2);
	strncpy(full_name, name_s, strlen(name_s));
	full_name[strlen(name_s)] = '.';
	strncpy(full_name + strlen(name_s) + 1, name,
	        strlen(name) + 1);
	free(name_s);
	return full_name;
}

/* param stack */

static param_stack_t *
param_stack_create (void)
{
	param_stack_t *p = (param_stack_t *) malloc(sizeof(param_stack_t));
	p->n = 0;
	p->cap = PARAM_BLK_SIZE;
	p->params = (param_t *) calloc(p->cap, sizeof(param_t));
	return p;
}

static void
param_stack_leave (param_stack_t *p)
{
	p->n--;
}

static int
param_stack_to_te_vars (param_stack_t *p, te_variable **vars)
{
	int VARS_BLK_SIZE = 32;
	int te_vars_n = 0;
	int te_vars_cap = VARS_BLK_SIZE;
	*vars = (te_variable *) calloc(te_vars_cap, sizeof(te_variable));

	/* add starting from the tail -- from the most recent */
	for (int i = p->n - 1; i >= 0; i--) {
		bool is_present = false;
		int param_name_len = strlen(p->params[i].name);
		for (int j = 0; j < te_vars_n; j++) {
			if (strncmp(p->params[i].name, (*vars)[j].name, param_name_len)
				== 0)
			{
				is_present = true;
				break;
			}
		}
		if (is_present) {
			continue;
		}
		if (te_vars_n == te_vars_cap) {
			te_vars_cap += VARS_BLK_SIZE;
			*vars = (te_variable *) realloc(*vars,
				te_vars_cap * sizeof(te_variable));
		}
		(*vars)[te_vars_n].name = p->params[i].name;
		(*vars)[te_vars_n].address = &p->params[i].value;
		te_vars_n++;
	}

	return te_vars_n;
}

static double
param_stack_eval (param_stack_t *p, char *value)
{
	te_variable *vars = NULL;
	int vars_n = param_stack_to_te_vars(p, &vars);
	int err;
	te_expr *e = te_compile(value, vars, vars_n, &err);
	if (e == NULL) {
		error("could not evaluate %s (error near %d)\n", value, err);
		return 0.0;
	} else {
		return te_eval(e);
	}
}

static void
param_stack_enter (param_stack_t *p, raw_param_t *r)
{
	if (p->n == p->cap) {
		p->cap += PARAM_BLK_SIZE;
		p->params = (param_t *) realloc(p->params, p->cap * sizeof(param_t));
	}

	/* param stack's lifetime is contained in raw params' lifetime */
	p->params->name = r->name;
	p->params->value = param_stack_eval(p, r->value);
	p->n++;
}

static void
param_stack_destroy (param_stack_t *p)
{
	free(p);
}

/* module to graph conversion */

static module_t *
find_module (network_definition_t *net, char *name)
{
	for (int i = 0; i < net->n_modules; i++)
		if (strcmp(net->modules[i].name, name) == 0)
			return &net->modules[i];
	return NULL;
}

static void
expand_module (graph_t *g, module_t *module, network_definition_t *net,
               name_stack_t *stack, param_stack_t *p)
{
	for (int i = 0; i < module->n_params; i++) {
		param_stack_enter(p, &module->params[i]);
	}
	if (module->submodules == NULL) {
		char *name_s = name_stack_name(stack);
		graph_add_node(g, name_s, NODE_NODE);
		for (int i = 0; i < module->n_gates; i++) {
			char *full_name = get_full_name(stack, module->gates[i]);
			graph_add_edge(g, full_name, name_s, NODE_GATE);
		}
		free(name_s);
		return;
	}
	for (int i = 0; i < module->n_submodules; i++) {
		submodule_t smodule = module->submodules[i];
		for (int i = 0; i < smodule.n_params; i++) {
			param_stack_enter(p, &smodule.params[i]);
		}
		int size;
		if (smodule.size == NULL) {
			size = 0;
		} else {
			sscanf(smodule.size, "%d", &size);
		}
		if (size > 0) {
			for (int j = 0; j < size; j++) {
				name_stack_enter(stack, smodule.name, j);
				module_t *module = find_module(net,
				                               smodule.module);
				expand_module(g, module, net, stack, p);
				name_stack_leave(stack);
			}
		} else {
			name_stack_enter(stack, smodule.name, -1);
			module_t *module = find_module(net, smodule.module);
			expand_module(g, module, net, stack, p);
			name_stack_leave(stack);
		}
		for (int i = 0; i < smodule.n_params; i++) {
			param_stack_leave(p);
		}
	}
	for (int i = 0; i < module->n_connections; i++) {
		int l = strlen(module->connections[i]);
		char *name_a = (char *) malloc(l + 1);
		char *name_b = (char *) malloc(l + 1);
		sscanf(module->connections[i], "%s %s", name_a, name_b);

		char *full_name_a = get_full_name(stack, name_a);
		char *full_name_b = get_full_name(stack, name_b);
		graph_add_edge(g, full_name_a, full_name_b, NODE_GATE);
		free(full_name_a);
		free(full_name_b);
		free(name_a);
		free(name_b);
	}
	for (int i = 0; i < module->n_params; i++) {
		param_stack_leave(p);
	}
}

static graph_t *
definition_to_graph (network_definition_t *net)
{
	module_t *root_module = find_module(net, net->network->module);
	graph_t *g = graph_create();
	name_stack_t *stack = name_stack_create("network");
	param_stack_t *p = param_stack_create();
	for (int i = 0; i < net->network->n_params; i++) {
		param_stack_enter(p, &net->network->params[i]);
	}
	expand_module(g, root_module, net, stack, p);
	for (int i = 0; i < net->network->n_params; i++) {
		/* not required, can be removed for optimization */
		param_stack_leave(p);
	}
	param_stack_destroy(p);
	free(stack);
	return g;
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
	graph_print(gg, stdout);
	graph_destroy(gg);

	exit(EXIT_SUCCESS);
}
