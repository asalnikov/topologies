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
#include "graph.h"
#include "defs.h"

/* TODO
 * nested loops
 * param vectors
 * compact network form
 * rewrite malloc
 */

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
get_full_name (name_stack_t *s, char *name, int index)
{
	int added_len = 0;
	if (index != -1) {
		added_len = snprintf(0, 0, "[%d]", index);
	}
	char *name_s = name_stack_name(s);
	char *full_name =
		(char *) malloc(strlen(name) + strlen(name_s) + added_len + 2);
	strncpy(full_name, name_s, strlen(name_s));
	full_name[strlen(name_s)] = '.';
	strncpy(full_name + strlen(name_s) + 1, name,
	        strlen(name) + 1);
	if (index != -1) {
		snprintf(full_name + strlen(name_s) + strlen(name) + 1,
			added_len + 1, "[%d]", index);
	}
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
			if (strncmp(p->params[i].name, (*vars)[j].name,
				param_name_len) == 0)
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
	}
	double res = te_eval(e);
	te_free(e);
	free(vars);
	return res;
}

static void
param_stack_enter (param_stack_t *p, raw_param_t *r)
{
	if (p->n == p->cap) {
		p->cap += PARAM_BLK_SIZE;
		p->params = (param_t *) realloc(p->params,
			p->cap * sizeof(param_t));
	}

	/* param stack's lifetime is contained in raw params' lifetime */
	p->params[p->n].name = r->name;
	p->params[p->n].value = param_stack_eval(p, r->value);
	p->n++;
}

static void
param_stack_enter_val (param_stack_t *p, char *name, int d)
{
	if (p->n == p->cap) {
		p->cap += PARAM_BLK_SIZE;
		p->params = (param_t *) realloc(p->params,
			p->cap * sizeof(param_t));
	}

	/* param stack's lifetime is contained in name's lifetime */
	p->params[p->n].name = name;
	p->params[p->n].value = d;
	p->n++;
}

static void
param_stack_destroy (param_stack_t *p)
{
	free(p->params);
	free(p);
}

/* static void
param_stack_print (param_stack_t *p, FILE *stream)
{
	for (int i = 0; i < p->n; i++) {
		fprintf(stream, "%s = %f, ", p->params[i].name,
			p->params[i].value);
	}
} */

static char *
eval_conn_name (param_stack_t *p, char *name)
{
	int len = strlen(name);
	char *left, *right;

	int n_params = 0;
	left = name;
	while ((left = strchr(left, '[')) != NULL) {
		left++;
		n_params++;
	}

	int *eval_res = calloc(n_params, sizeof(int));
	int i = 0;
	int added_len = 0;

	left = name;
	while ((left = strchr(left, '[')) != NULL) {
		right = strchr(left, ']');
		if (right == NULL) {
			free(eval_res);
			return NULL;
		}
		*right = 0;
		left++;	
		eval_res[i] = lrint(param_stack_eval(p, left));
		*right = ']';
		added_len += snprintf(0, 0, "%d", eval_res[i]);
		i++;
	}

	char *res_str = calloc(len + added_len + 1, sizeof(char));
	char *t = res_str;
	left = name;
	char *prev = left;
	i = 0;
	while ((left = strchr(left, '[')) != NULL) {
		strncpy(t, prev, left - prev + 1);
		t += left - prev + 1;
		t += snprintf(t, res_str + len + added_len - t, "%d]",
			eval_res[i]);
		left = strchr(left, ']');
		prev = left + 1;
		i++;
	}
	strcpy(t, prev);
	free(eval_res);
	return res_str;
}

/* module to graph conversion */

static void
graph_eval_and_add_edge (graph_t *g, param_stack_t *p,
                         name_stack_t *stack,
                         connection_t *conn)
{
	char *r_name_a = conn->from;
	char *r_name_b = conn->to;
	char *name_a = eval_conn_name(p, r_name_a);
	char *name_b = eval_conn_name(p, r_name_b);
	if (name_a == NULL)
		error("could not evaluate %s\n", r_name_a);
	if (name_b == NULL)
		error("could not evaluate %s\n", r_name_b);
	char *full_name_a = get_full_name(stack, name_a, -1);
	char *full_name_b = get_full_name(stack, name_b, -1);
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
add_gate (graph_t *g, name_stack_t *stack, char *name_s,
          gate_t *gate, int j)
{
	char *full_name = get_full_name(stack, gate->name, j);
	graph_add_node(g, full_name, NODE_GATE);
	if (graph_add_edge_name(g, full_name, name_s) < 0) {
		error("could not connect %s and %s\n",
			full_name, name_s);
	}
	free(full_name);
}

static void
expand_module (graph_t *g, module_t *module, network_definition_t *net,
               name_stack_t *stack, param_stack_t *p);

static void
enter_and_expand_module(network_definition_t *net, graph_t *g,
                        name_stack_t *stack, param_stack_t *p,
                        submodule_t *smodule, int j)
{
	name_stack_enter(stack, smodule->name, j);
	module_t *module = find_module(net, smodule->module);
	if (module == NULL) {
		error("no such module: %s\n", smodule->module);
	}
	expand_module(g, module, net, stack, p);
	name_stack_leave(stack);
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
			int size = lrint(param_stack_eval(p, module->gates[i].size));
			if (size == 0) {
				add_gate(g, stack, name_s, &module->gates[i], -1);
			} else {
				for (int j = 0; j < size; j++) {
					add_gate(g, stack, name_s, &module->gates[i], j);
				}
			}
		}
		free(name_s);
	} else {
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
					enter_and_expand_module(net, g, stack,
						p, &smodule, j);
				}
			} else {
				enter_and_expand_module(net, g, stack,
					p, &smodule, -1);
			}
			for (int i = 0; i < smodule.n_params; i++) {
				param_stack_leave(p);
			}
		}
		for (int i = 0; i < module->n_connections; i++) {
			if (module->connections[i].loop != NULL) {
				int start = lrint(param_stack_eval(p,
					module->connections[i].start));
				int end = lrint(param_stack_eval(p,
					module->connections[i].end));
				if (start > end) {
					error("%d > %d\n", start, end);
				}
				for (int j = start; j <= end; j++) {
					param_stack_enter_val(p,
						module->connections[i].loop, j);
					graph_eval_and_add_edge(g, p, stack,
						&module->connections[i]);
					param_stack_leave(p);
				}
			} else {
				graph_eval_and_add_edge(g, p, stack,
					&module->connections[i]);
			}
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
	name_stack_t *stack = name_stack_create("network");
	param_stack_t *p = param_stack_create();
	for (int i = 0; i < net->network->n_params; i++) {
		param_stack_enter(p, &net->network->params[i]);
	}
	expand_module(g, root_module, net, stack, p);
	for (int i = 0; i < net->network->n_params; i++) {
		/* not required */
		param_stack_leave(p);
	}
	param_stack_destroy(p);
	free(stack->name);
	free(stack);
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
	/*
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
			free(n->modules[i].connections[j].from);
			free(n->modules[i].connections[j].to);
			free(n->modules[i].connections[j].loop);
			free(n->modules[i].connections[j].start);
			free(n->modules[i].connections[j].end);
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
