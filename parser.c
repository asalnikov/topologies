#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "jsmn.h"
#include "defs.h"

typedef enum {
	STATE_START,
	STATE_BETWEEN,
	STATE_MODULE,
	STATE_MODULE_PARAMS,
	STATE_MODULE_SUBMODULES_BETWEEN,
	STATE_MODULE_SUBMODULE,
	STATE_MODULE_SUBMODULE_PARAMS,
	STATE_MODULE_CONNECTIONS_BETWEEN,
	STATE_MODULE_CONNECTION,
	STATE_MODULE_GATES_BETWEEN,
	STATE_MODULE_GATE,
	STATE_NETWORK,
	STATE_NETWORK_PARAMS
} state_t;

static bool
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

static void
json_str_cpy (const char *json, jsmntok_t *tok, char **t)
{
	int s_len = tok->end - tok->start;
	*t = calloc(s_len + 1, 1);
	strncpy(*t, json + tok->start, s_len);
}


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
	case STATE_MODULE_CONNECTIONS_BETWEEN:
		state_name = "STATE_MODULE_CONNECTIONS_BETWEEN";
		break;
	case STATE_MODULE_CONNECTION:
		state_name = "STATE_MODULE_CONNECTION";
		break;
	case STATE_MODULE_GATES_BETWEEN:
		state_name = "STATE_MODULE_GATES_BETWEEN";
		break;
	case STATE_MODULE_GATE:
		state_name = "STATE_MODULE_GATE";
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

static void
parse_connection (int subobj_n, jsmntok_t *tokens, char *text,
                  connection_wrapper_t *connection, int *i)
{
	/* at the start, i points at the outermost object */
	int state = STATE_MODULE_CONNECTION;
	if ((subobj_n != 2) && (subobj_n != 4)) {
		bad_token(*i, tokens[*i].type, tokens[*i].start, text, state);
	}

	*i += 1;
	if (subobj_n == 2) {
		connection->type = CONN_HAS_CONN;
		connection->ptr.conn = calloc(1, sizeof(connection_plain_t));
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "from")) {
				if (connection->ptr.conn->from != NULL)
					bad_token(*i, tokens[*i].type,
						  tokens[*i].start, text, state);
				json_str_cpy(text, &tokens[*i + 1],
					     &connection->ptr.conn->from);
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "to")) {
				if (connection->ptr.conn->to != NULL)
					bad_token(*i, tokens[*i].type,
						  tokens[*i].start, text, state);
				json_str_cpy(text, &tokens[*i + 1],
					     &connection->ptr.conn->to);
				*i += 2;
			} else {
				bad_token(*i, tokens[*i].type, tokens[*i].start,
					 text, state);
			}
		}
	} else if (subobj_n == 4) {
		connection->type = CONN_HAS_LOOP;
		connection->ptr.loop = calloc(1, sizeof(connection_loop_t));
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "start")) {
				if (connection->ptr.loop->start != NULL)
					bad_token(*i, tokens[*i].type,
						  tokens[*i].start, text, state);
				json_str_cpy(text, &tokens[*i + 1],
					     &connection->ptr.loop->start);
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "end")) {
				if (connection->ptr.loop->end != NULL)
					bad_token(*i, tokens[*i].type,
						  tokens[*i].start, text, state);
				json_str_cpy(text, &tokens[*i + 1],
					     &connection->ptr.loop->end);
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "loop")) {
				if (connection->ptr.loop->loop != NULL)
					bad_token(*i, tokens[*i].type,
						  tokens[*i].start, text, state);
				json_str_cpy(text, &tokens[*i + 1],
					     &connection->ptr.loop->loop);
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "conn")) {
				*i += 1;
				if ((tokens[*i].type != JSMN_OBJECT) ||
				     (connection->ptr.loop->conn != NULL))
				{
					printf("###\n");
					bad_token(*i, tokens[*i].type,
					          tokens[*i].start,
					          text, state);
				}
				connection->ptr.loop->conn =
					calloc(1, sizeof(connection_wrapper_t));
				parse_connection(tokens[*i].size, tokens, text,
				                 connection->ptr.loop->conn, i);
			} else {
				bad_token(*i, tokens[*i].type, tokens[*i].start,
					 text, state);
			}
		}
	}
}

static void
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
                  network_definition_t *net)
{
	module_t *m = NULL;
	network_t *network = NULL;
	int m_n = 0;
	int m_i = -1;
	int i = 0;
	int arr_i = 0;
	int arr_n = 0;
	int obj_i = 0;
	int obj_n = 0;
	int subarr_i = 0;
	int subarr_n = 0;
	int subobj_i = 0;
	int subobj_n = 0;
	state_t state = STATE_START;

	do {
		if (state == STATE_START) {
			if (tokens[i].type != JSMN_ARRAY)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			m_n = tokens[i].size;
			m_i = net->n_modules - 1;
			m = realloc(net->modules,
			            (net->n_modules + m_n) * sizeof(module_t));
			memset(m + net->n_modules, 0,
			       m_n * sizeof(module_t));
			net->n_modules += m_n;
			net->modules = m;
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
				m_i += 1;
				state = STATE_MODULE;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				obj_n = tokens[i].size;
				obj_i = 0;
				i += 1;
			} else if (json_str_eq(text, &tokens[i + 1], "network")) {
				state = STATE_NETWORK;
				i += 2;
				net->n_modules -= 1;
				m_n -= 1;
				if ((tokens[i].type != JSMN_OBJECT) ||
				    (net->network != NULL))
				{
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				}
				network = calloc(1, sizeof(network_t));
				net->network = network;
				obj_n = tokens[i].size;
				obj_i = 0;
				i += 1;
			} else {
				bad_token(i, tokens[i + 1].type,
				          tokens[i + 1].start, text, state);
			}

		} else if (state == STATE_MODULE) {
			if (obj_i == obj_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "name")) {
				if (m[m_i].name != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].name);
				obj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				state = STATE_MODULE_PARAMS;
			} else if (json_str_eq(text, &tokens[i], "submodules")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (m[m_i].submodules != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				if (arr_n > 0)
					m[m_i].submodules =
						calloc(arr_n, sizeof(submodule_t));
				m[m_i].n_submodules = arr_n;
				i += 1;
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "connections")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				if (m[m_i].connections != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start,
					          text, state);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (arr_n > 0)
					m[m_i].connections =
						calloc(arr_n,
						sizeof(connection_wrapper_t));
				m[m_i].n_connections = arr_n;
				i += 1;
				state = STATE_MODULE_CONNECTIONS_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "gates")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text,
					          state);
				if (m[m_i].gates != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start,
					          text, state);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (arr_n > 0)
					m[m_i].gates =
						calloc(arr_n, sizeof(gate_t));
				m[m_i].n_gates = arr_n;
				i += 1;
				state = STATE_MODULE_GATES_BETWEEN;
			} else {
				bad_token(i, tokens[i].type, tokens[i].start,
				         text, state);
			}

		} else if (state == STATE_MODULE_PARAMS) {
			arr_n = tokens[i].size;
			if (m[m_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			if (arr_n > 0) {
				m[m_i].params =
					calloc(arr_n, sizeof(raw_param_t));
			}
			m[m_i].n_params = arr_n;
			i += 1;
			for (arr_i = 0; arr_i < arr_n; arr_i++) {
				if ((tokens[i].type != JSMN_OBJECT) ||
				    (tokens[i].size != 1) ||
				    (tokens[i + 1].type != JSMN_STRING) ||
				    (tokens[i + 2].type != JSMN_STRING))
				{
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				}
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].params[arr_i].name);
				json_str_cpy(text, &tokens[i + 2],
				             &m[m_i].params[arr_i].value);
				i += 3;
			}
			state = STATE_MODULE;

		} else if (state == STATE_MODULE_GATES_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			} else {
				subobj_i = 0;
				subobj_n = tokens[i].size;
				state = STATE_MODULE_GATE;
				i += 1;
			}
			
		} else if (state == STATE_MODULE_CONNECTIONS_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			} else {
				state = STATE_MODULE_CONNECTION;
			}

		} else if (state == STATE_MODULE_SUBMODULES_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			} else {
				subobj_i = 0;
				subobj_n = tokens[i].size;
				state = STATE_MODULE_SUBMODULE;
				i += 1;
			}

		} else if (state == STATE_MODULE_CONNECTION) {
			subobj_i = 0;
			subobj_n = tokens[i].size;
			parse_connection(subobj_n, tokens, text,
			                 &m[m_i].connections[arr_i],
					 &i);
			state = STATE_MODULE_CONNECTIONS_BETWEEN;
			arr_i += 1;

		} else if (state == STATE_MODULE_SUBMODULE) {
			if (subobj_i == subobj_n) {
				state = STATE_MODULE_SUBMODULES_BETWEEN;
				arr_i += 1;
			} else if (json_str_eq(text, &tokens[i], "name")) {
				if (m[m_i].submodules[arr_i].name != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].submodules[arr_i].name);
				subobj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (m[m_i].submodules[arr_i].module != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].submodules[arr_i].module);
				subobj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "size")) {
				if (m[m_i].submodules[arr_i].size != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].submodules[arr_i].size);
				subobj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				subobj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				state = STATE_MODULE_SUBMODULE_PARAMS;
			} else {
				bad_token(i, tokens[i].type, tokens[i].start,
				         text, state);
			}
		} else if (state == STATE_MODULE_SUBMODULE_PARAMS) {
			subarr_n = tokens[i].size;
			if (m[m_i].submodules[arr_i].params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			if (arr_n > 0)
				m[m_i].submodules[arr_i].params =
					calloc(arr_n, sizeof(raw_param_t));
			m[m_i].submodules[arr_i].n_params = subarr_n;
			for (subarr_i = 0; subarr_i < subarr_n; subarr_i++) {
				if ((tokens[i].type != JSMN_OBJECT) ||
				    (tokens[i].size != 1) ||
				    (tokens[i + 1].type != JSMN_STRING) ||
				    (tokens[i + 2].type != JSMN_STRING))
				{
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				}
				json_str_cpy(text, &tokens[i + 1],
				             &m[m_i].submodules[arr_i].params[subarr_i].name);
				json_str_cpy(text, &tokens[i + 2],
				             &m[m_i].submodules[arr_i].params[subarr_i].value);
				i += 3;
			}
			i += 1;
			state = STATE_MODULE_SUBMODULE;

		} else if (state == STATE_MODULE_GATE) {
			if (subobj_i == subobj_n) {
				state = STATE_MODULE_GATES_BETWEEN;
				arr_i += 1;
			} else {
				json_str_cpy(text, &tokens[i],
					     &m[m_i].gates[arr_i].name);
				json_str_cpy(text, &tokens[i + 1],
					     &m[m_i].gates[arr_i].size);
				i += 2;
				subobj_i += 1;
			}

		} else if (state == STATE_NETWORK) {
			if (obj_i == obj_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (network->module != NULL)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				json_str_cpy(text, &tokens[i + 1],
				             &network->module);
				obj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				state = STATE_NETWORK_PARAMS;
			} else {
				bad_token(i, tokens[i].type, tokens[i].start,
				         text, state);
			}

		} else if (state == STATE_NETWORK_PARAMS) {
			arr_n = tokens[i].size;
			if (network->params != NULL)
				bad_token(i, tokens[i].type, tokens[i].start,
				          text, state);
			if (arr_n > 0)
				network->params = calloc(arr_n, sizeof(raw_param_t));
			network->n_params = arr_n;
			for (arr_i = 0; arr_i < arr_n; arr_i++) {
				if ((tokens[i].type != JSMN_OBJECT) ||
				    (tokens[i].size != 1) ||
				    (tokens[i + 1].type != JSMN_STRING) ||
				    (tokens[i + 2].type != JSMN_STRING))
				{
					bad_token(i, tokens[i].type,
					          tokens[i].start, text, state);
				}
				json_str_cpy(text, &tokens[i + 1],
				             &network->params[arr_i].name);
				json_str_cpy(text, &tokens[i + 2],
				             &network->params[arr_i].value);
				i += 3;
			}
			i += 1;
			state = STATE_NETWORK;
		}
	} while (i < n_tokens);

	return;
}
/*
static void
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
*/
int
json_read_file (char *text, off_t file_size,
                network_definition_t *net)
{

	jsmntok_t *tokens;
	jsmn_parser parser;
	jsmn_init(&parser);
	int n_tokens = jsmn_parse(&parser, text, file_size, NULL, 0);
	if (n_tokens < 0)
		return n_tokens;
	tokens = malloc(sizeof(jsmntok_t) * (size_t) n_tokens);
	jsmn_init(&parser);
	jsmn_parse(&parser, text, file_size, tokens, n_tokens);
	/* json_print(tokens, n_tokens, text); */

	json_deserialize(tokens, n_tokens, text, net);
	free(tokens);
	return 0;
}
