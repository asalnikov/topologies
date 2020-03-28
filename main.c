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
json_str_eq(const char *json, jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING &&
	    (int) strlen(s) == tok->end - tok->start &&
	    strncmp(json + tok->start, s, tok->end - tok->start) == 0)
	{
		return 0;
	}
	return false;
}

void
json_str_cpy(const char *json, jsmntok_t *tok, char **t)
{
	int s_len = tok->end - tok->start;
	*t = calloc(s_len + 1, 1);
	strncpy(*t, json + tok->start, s_len);
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
bad_token (int type, int start_pos)
{
	fprintf(stderr, "unexpected %s token at position %d\n",
	                token_type_name(type), start_pos);
	exit(EXIT_FAILURE);
}

void
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
                  module_t **modules, int *n_modules)
{
	module_t *modules = NULL;
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
				bad_token(tokens[i].type, tokens[i].start);
			modules_n = tokens[i].size;
			modules = calloc(modules_n, sizeof(module_t));
			i++;
			state = STATE_BETWEEN;

		} else if (state == STATE_BETWEEN) {
			if (tokens[i].type != JSMN_OBJECT ||
			    tokens[i].size != 1)
			{
				bad_token(tokens[i].type, tokens[i].start);
			}
			if json_str_eq(text, &tokens[i + 1], "module") {
				modules_i += 1;
				state = STATE_MODULE;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(tokens[i].type, tokens[i].start);
				object_n = tokens[i].size;
				object_i = 0;
			} else if json_str_eq(text, &tokens[i + 1], "network") {
				state = STATE_NETWORK;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(tokens[i].type, tokens[i].start);
				object_n = tokens[i].size;
				object_i = 0;
			} else {
				bad_token(tokens[i + 1].type,
				          tokens[i + 1].start);
			}

		} else if (state == STATE_MODULE) {
			if (object_i == object_n) {
				state = STATE_BETWEEN;
			} else if json_str_eq(text, &tokens[i], "name") {
				if (modules[modules_i].name != NULL)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].name);
				object_i += 1;
				i += 2;
			} else if json_str_eq(text, &tokens[i], "params") {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(tokens[i].type, tokens[i].start);
				state = STATE_MODULE_PARAMS;
			} else if json_str_eq(text, &tokens[i], "submodules") {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(tokens[i].type, tokens[i].start);
				array_n = tokens[i].size;
				array_i = 0;
				if (modules[modules_i].submodules != NULL)
					bad_token(tokens[i].type, tokens[i].start);
				modules[modules_i].submodules = calloc(array_n,
								       sizeof(submodule_t));
				i += 1;
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if json_str_eq(text, &tokens[i], "connections") {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(tokens[i].type, tokens[i].start);
				state = STATE_MODULE_CONNECTIONS;
			} else if json_str_eq(text, &tokens[i], "gates") {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(tokens[i].type, tokens[i].start);
				state = STATE_MODULE_GATES;
			}
		} else if (state == STATE_MODULE_PARAMS) {
			array_n = tokens[i].size;
			if (modules[modules_i].params != NULL)
				bad_token(tokens[i].type, tokens[i].start);
			modules[modules_i].params = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].params[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_GATES) {
			array_n = tokens[i].size;
			if (modules[modules_i].gates != NULL)
				bad_token(tokens[i].type, tokens[i].start);
			modules[modules_i].gates = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].gates[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_CONNECTIONS) {
			array_n = tokens[i].size;
			if (modules[modules_i].connections != NULL)
				bad_token(tokens[i].type, tokens[i].start);
			modules[modules_i].connections = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].connections[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_SUBMODULES_BETWEEN) {
			if (array_i == array_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(tokens[i].type, tokens[i].start);
			} else {
				subobject_i = 0;
				subobject_n = tokens[i].size;
				state = STATE_MODULE_SUBMODULE;
				i += 1;
			}
		} else if (state == STATE_MODULE_SUBMODULE) {
			if (subobject_i == subobject_n) {
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if json_str_eq(text, &tokens[i], "name") {
				if (modules[modules_i].submodules[array_i].name != NULL)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].name);
				subobject_i += 1;
				i += 2;
			} else if json_str_eq(text, &tokens[i], "module") {
				if (modules[modules_i].submodules[array_i].module != NULL)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].module);
				subobject_i += 1;
				i += 2;
			} else if json_str_eq(text, &tokens[i], "size") {
				if (modules[modules_i].submodules[array_i].size != NULL)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].size);
				subobject_i += 1;
				i += 2;
			} else if json_str_eq(text, &tokens[i], "params") {
				subobject_i += 1;
				i += 1;
				// TODO
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(tokens[i].type, tokens[i].start);
				state = STATE_MODULE_SUBMODULE_PARAMS;
			}
		} else if (state == STATE_MODULE_SUBMODULE_PARAMS) {
			subarray_n = tokens[i].size;
			if (modules[modules_i].submodules[array_i].params != NULL)
				bad_token(tokens[i].type, tokens[i].start);
			modules[modules_i].submodules[array_i].params =
				calloc(array_n, sizeof(char *));
			for (subarray_i = 0; subarray_i < subarray_n; subarray_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(tokens[i].type, tokens[i].start);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].submodules[array_i].params[subarray_i]);
			}
			i += 1;
			state = STATE_MODULE_SUBMODULE;
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
		printf("type: %s, size: %d, data:\n", type, tokens[i].size);
		printf("%.*s\n\n", tokens[i].end - tokens[i].start,
		                   text + tokens[i].start);
	}
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

	json_print(tokens, n_tokens, addr);
	//module_t *modules = NULL;
	//int n_modules = 0;
	//json_deserialize(tokens, n_tokens, addr, &modules, &n_modules);

	exit(EXIT_SUCCESS);
}
