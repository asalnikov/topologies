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

#include "parser.h"
#include "defs.h"

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

static char *
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

static char *
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

static void
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

static void
bad_token (int i, int type, int start_pos, char *text, state_t state)
{
	fprintf(stderr, "unexpected %s token #%d at ", token_type_name(type), i);
	print_line_number(text, start_pos);
	fprintf(stderr, " state %s\n", state_name(state));
	exit(EXIT_FAILURE);
}

void
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
                  network_definition_t *network_definition)
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
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			modules_n = tokens[i].size;
			modules = calloc(modules_n, sizeof(module_t));
			network_definition->n_modules = modules_n;
			network_definition->modules = modules;
			network = calloc(1, sizeof(network_t));
			network_definition->network = network;
			i++;
			state = STATE_BETWEEN;

		} else if (state == STATE_BETWEEN) {
			if (tokens[i].type != JSMN_OBJECT ||
			    tokens[i].size != 1)
			{
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			}
			if (json_str_eq(text, &tokens[i + 1], "module")) {
				modules_i += 1;
				state = STATE_MODULE;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				object_n = tokens[i].size;
				object_i = 0;
				i += 1;
			} else if (json_str_eq(text, &tokens[i + 1], "network")) {
				state = STATE_NETWORK;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
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
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].name);
				object_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				state = STATE_MODULE_PARAMS;
			} else if (json_str_eq(text, &tokens[i], "submodules")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				array_n = tokens[i].size;
				array_i = 0;
				if (modules[modules_i].submodules != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				modules[modules_i].submodules = calloc(array_n,
								       sizeof(submodule_t));
				i += 1;
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "connections")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				state = STATE_MODULE_CONNECTIONS;
			} else if (json_str_eq(text, &tokens[i], "gates")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				state = STATE_MODULE_GATES;
			} else {
				bad_token(i, tokens[i].type, tokens[i].start,
				         text, state);
			}
		} else if (state == STATE_MODULE_PARAMS) {
			array_n = tokens[i].size;
			if (modules[modules_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			modules[modules_i].params = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].params[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_GATES) {
			array_n = tokens[i].size;
			if (modules[modules_i].gates != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			modules[modules_i].gates = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].gates[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_CONNECTIONS) {
			array_n = tokens[i].size;
			if (modules[modules_i].connections != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			modules[modules_i].connections = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				json_str_cpy(text, &tokens[i],
				             &modules[modules_i].connections[array_i]);
			}
			i += 1;
			state = STATE_MODULE;
		} else if (state == STATE_MODULE_SUBMODULES_BETWEEN) {
			if (array_i == array_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
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
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].name);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (modules[modules_i].submodules[array_i].module != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].module);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "size")) {
				if (modules[modules_i].submodules[array_i].size != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &modules[modules_i].submodules[array_i].size);
				subobject_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				subobject_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				state = STATE_MODULE_SUBMODULE_PARAMS;
			}
		} else if (state == STATE_MODULE_SUBMODULE_PARAMS) {
			subarray_n = tokens[i].size;
			if (modules[modules_i].submodules[array_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			modules[modules_i].submodules[array_i].params =
				calloc(array_n, sizeof(char *));
			for (subarray_i = 0; subarray_i < subarray_n; subarray_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
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
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &network->module);
				object_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				object_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				state = STATE_NETWORK_PARAMS;
			}
		} else if (state == STATE_NETWORK_PARAMS) {
			array_n = tokens[i].size;
			if (network->params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			network->params = calloc(array_n, sizeof(char *));
			for (array_i = 0; array_i < array_n; array_i++) {
				i += 1;
				if (tokens[i].type != JSMN_STRING)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
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
