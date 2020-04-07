#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "jsmn.h"

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

bool
json_str_eq (const char *json, jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING &&
	    (int) strlen(s) == tok->end - tok->start &&
	    strncmp(json + tok->start, s, tok->end - tok->start) == 0)
	{
		return true;
	}
	return false;
}

void
json_str_cpy (const char *json, jsmntok_t *tok, char **t)
{
	int s_len = tok->end - tok->start;
	*t = calloc(s_len + 1, 1);
	strncpy(*t, json + tok->start, s_len);
}

typedef struct node_list node_list_t;

typedef struct {
	char *name;
	node_list_t *adj;
	int n;
} node_t;

struct node_list {
	node_t *node;
	node_list_t *next;
};

typedef struct {
	node_t *nodes;
	int n_nodes;
	int cap_nodes;
} graph_t;

enum { GRAPH_BLK_SIZE = 16 };

graph_t *
graph_create ()
{
	graph_t *g = malloc(sizeof(graph_t));
	g->n_nodes = 0;
	g->cap_nodes = GRAPH_BLK_SIZE;
	g->nodes = calloc(g->cap_nodes, sizeof(node_t));
	return g;
}

void
graph_add_node (graph_t *g, char *name)
{
	int i = g->n_nodes;
	g->n_nodes++;
	if (g->n_nodes == g->cap_nodes) {
		g->cap_nodes += GRAPH_BLK_SIZE;
		g->nodes = realloc(g->nodes, g->cap_nodes);
	}
	g->nodes[i].name = malloc(strlen(name) + 1);
	strncpy(g->nodes[i].name, name, strlen(name) + 1);
	g->nodes[i].adj = NULL;
	g->nodes[i].n = i;
}

node_t *
graph_find_node (graph_t *g, char *name)
{
	for (int i = 0; i < g->n_nodes; i++) {
		if (strcmp(g->nodes[i].name, name) == 0)
			return &(g->nodes[i]);
	}
	return NULL;
}

void
graph_add_edge (graph_t *g, char *name_a, char *name_b)
{
	node_t *node_a, *node_b;
	node_a = graph_find_node(g, name_a);
	node_b = graph_find_node(g, name_b);
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

typedef enum {
	NUMBER,
	PARAM
} num_or_param;

typedef union {
	int number_size;
	char *param_size;
} vec_size_t;

typedef struct {
	char *name;
	num_or_param type;
	vec_size_t size;
} gate_t;

typedef struct {
	char *name;
	num_or_param type;
	vec_size_t size;
} param_t;

typedef struct {
	char *name;
	char *module;
	char *size;
	//param_t *params;
	char **params;
} submodule_t;

typedef struct {
	char *name;
	char **params;
	submodule_t *submodules;
	//gate_t *gates;
	char **gates;
	char **connections;
} module_t;

typedef struct {
	char *module;
	char **params;
} network_t;

typedef enum {
	STATE_START,
	STATE_BETWEEN,
	STATE_MODULE,
	STATE_MODULE_PARAMS,
	STATE_MODULE_SUBMODULES_BETWEEN,
	STATE_MODULE_SUBMODULE,
	STATE_MODULE_SUBMODULE_PARAMS,
	STATE_MODULE_CONNECTIONS,
	STATE_MODULE_GATES,
	STATE_NETWORK,
	STATE_NETWORK_PARAMS
} state_t;

char *
state_name (state_t state)
{
	char *state_name = 0;
	switch (state) {
	case STATE_START:
		state_name = "STATE_START";
		break;
	case STATE_BETWEEN:
		state_name = "STATE_BETWEEN";
		break;
	case STATE_MODULE:
		state_name = "STATE_MODULE";
		break;
	case STATE_MODULE_PARAMS:
		state_name = "STATE_MODULE_PARAMS";
		break;
	case STATE_MODULE_SUBMODULES_BETWEEN:
		state_name = "STATE_MODULE_SUBMODULES_BETWEEN";
		break;
	case STATE_MODULE_SUBMODULE:
		state_name = "STATE_MODULE_SUBMODULE";
		break;
	case STATE_MODULE_SUBMODULE_PARAMS:
		state_name = "STATE_MODULE_SUBMODULE_PARAMS";
		break;
	case STATE_MODULE_CONNECTIONS:
		state_name = "STATE_MODULE_CONNECTIONS";
		break;
	case STATE_MODULE_GATES:
		state_name = "STATE_MODULE_GATES";
		break;
	case STATE_NETWORK:
		state_name = "STATE_NETWORK";
		break;
	default:
		state_name = "STATE_NETWORK_PARAMS";
		break;
	}
	return state_name;
}

char *
token_type_name (int type)
{
	char *type_name = 0;
	switch (type) {
	case JSMN_PRIMITIVE:
		type_name = "PRIMITIVE";
		break;
	case JSMN_OBJECT:
		type_name = "OBJECT";
		break;
	case JSMN_ARRAY:
		type_name = "ARRAY";
		break;
	case JSMN_STRING:
		type_name = "STRING";
		break;
	default:
		type_name = "UNDEF";
		break;
	}

	return type_name;
}

void
print_line_number (char *text, int start_pos)
{
	int line = 1;
	int pos_on_line = 1;
	for (int i = 0; i < start_pos; i++) {
		pos_on_line++;
		if (text[i] == '\n') {
			line++;
			pos_on_line = 1;
		}
	}
	fprintf(stderr, "line %d character %d", line, pos_on_line);
}

void
bad_token (int i, int type, int start_pos, char *text, state_t state)
{
	fprintf(stderr, "unexpected %s token #%d at ", token_type_name(type), i);
	print_line_number(text, start_pos);
	fprintf(stderr, " state %s\n", state_name(state));
	exit(EXIT_FAILURE);
}

void
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
                  module_t **out_modules, int *out_modules_n,
		  network_t **out_network)
{
	module_t *modules = NULL;
	network_t *network = NULL;
	int modules_n = 0;
	int modules_i = -1;
	int i = 0;
	int array_i = 0;
	int array_n = 0;
	int object_i = 0;
	int object_n = 0;
	int subarray_i = 0;
	int subarray_n = 0;
	int subobject_i = 0;
	int subobject_n = 0;
	state_t state = STATE_START;

	do {
		if (state == STATE_START) {
			if (tokens[i].type != JSMN_ARRAY)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			modules_n = tokens[i].size;
			modules = calloc(modules_n, sizeof(module_t));
			*out_modules_n = modules_n;
			*out_modules = modules;
			network = calloc(1, sizeof(network_t));
			*out_network = network;
			i++;
			state = STATE_BETWEEN;

		} else if (state == STATE_BETWEEN) {
			if (tokens[i].type != JSMN_OBJECT ||
			    tokens[i].size != 1)
			{
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			}
			if (json_str_eq(text, &tokens[i + 1], "module")) {
				modules_i += 1;
				state = STATE_MODULE;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				object_n = tokens[i].size;
				object_i = 0;
				i += 1;
			} else if (json_str_eq(text, &tokens[i + 1], "network")) {
				state = STATE_NETWORK;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				object_n = tokens[i].size;
				object_i = 0;
				i += 1;
			} else {
				bad_token(i, tokens[i + 1].type,
				          tokens[i + 1].start, text, state);
			}

		} else if (state == STATE_MODULE) {
			if (object_i == object_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "name")) {
				if (modules[modules_i].name != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].name);
				object_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				state = STATE_MODULE_PARAMS;
			} else if (json_str_eq(text, &tokens[i], "submodules")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				array_n = tokens[i].size;
				array_i = 0;
				if (modules[modules_i].submodules != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				modules[modules_i].submodules = calloc(array_n,
								       sizeof(submodule_t));
				i += 1;
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "connections")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				state = STATE_MODULE_CONNECTIONS;
			} else if (json_str_eq(text, &tokens[i], "gates")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				state = STATE_MODULE_GATES;
			} else {
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			}
		} else if (state == STATE_MODULE_PARAMS) {
			array_n = tokens[i].size;
			if (modules[modules_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			modules[modules_i].params = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].params[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_GATES) {
			array_n = tokens[i].size;
			if (modules[modules_i].gates != NULL)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			modules[modules_i].gates = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].gates[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_CONNECTIONS) {
			array_n = tokens[i].size;
			if (modules[modules_i].connections != NULL)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			modules[modules_i].connections = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].connections[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_SUBMODULES_BETWEEN) {
			if (array_i == array_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			} else {
				subobject_i = 0;
				subobject_n = tokens[i].size;
				state = STATE_MODULE_SUBMODULE;
				i += 1;
			}
		} else if (state == STATE_MODULE_SUBMODULE) {
			if (subobject_i == subobject_n) {
				state = STATE_MODULE_SUBMODULES_BETWEEN;
				array_i += 1;
			} else if (json_str_eq(text, &tokens[i], "name")) {
				if (modules[modules_i].submodules[array_i].name != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].name);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (modules[modules_i].submodules[array_i].module != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].module);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "size")) {
				if (modules[modules_i].submodules[array_i].size != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].size);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				subobject_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				state = STATE_MODULE_SUBMODULE_PARAMS;
			}
		} else if (state == STATE_MODULE_SUBMODULE_PARAMS) {
			subarray_n = tokens[i].size;
			if (modules[modules_i].submodules[array_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			modules[modules_i].submodules[array_i].params =
				calloc(array_n, sizeof(char *));
			for (subarray_i = 0; subarray_i < subarray_n; subarray_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].submodules[array_i].params[subarray_i]);
			}
			i += 1;
			state = STATE_MODULE_SUBMODULE;
		} else if (state == STATE_NETWORK) {
			if (object_i == object_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (network->module != NULL)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &network->module);
				object_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				state = STATE_NETWORK_PARAMS;
			}
		} else if (state == STATE_NETWORK_PARAMS) {
			array_n = tokens[i].size;
			if (network->params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start, text, state);
			network->params = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type, tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &network->params[array_i]);
			}
			i += 1;
			state = STATE_NETWORK;
		}
	} while (i < n_tokens);

	return;
}

void
json_print (jsmntok_t *tokens, int n_tokens, char *text)
{
	if (n_tokens == 0)
		return;

	for (int i = 0; i < n_tokens; i++) {
		char *type = token_type_name(tokens[i].type);
		printf("number: %d, type: %s, size: %d, data:\n", i, type, tokens[i].size);
		printf("%.*s\n\n", tokens[i].end - tokens[i].start,
		                   text + tokens[i].start);
	}
	fflush(stdout);
	return;
}

int
main (int argc, char *argv[])
{
	char *addr = NULL;

	off_t file_size = read_file(argc, argv, &addr);

	jsmntok_t *tokens;
	jsmn_parser parser;
	jsmn_init(&parser);
	int n_tokens = jsmn_parse(&parser, addr, file_size, NULL, 0);
	if (n_tokens < 0)
		error("%s: invalid JSON data: %d\n", argv[1], n_tokens);
		
	tokens = malloc(sizeof(jsmntok_t) * (size_t) n_tokens);
	jsmn_init(&parser);
	jsmn_parse(&parser, addr, file_size, tokens, n_tokens);

	//json_print(tokens, n_tokens, addr);
	network_t *network = NULL;
	module_t *modules = NULL;
	int n_modules = 0;
	json_deserialize(tokens, n_tokens, addr, &modules, &n_modules, &network);

	graph_t *g = graph_create();
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
	graph_destroy(g);

	exit(EXIT_SUCCESS);
}
