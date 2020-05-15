#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "jsmn.h"
#include "defs.h"
#include "errors.h"

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
	STATE_MODULE_REPLACE,
	STATE_MODULE_REPLACE_BETWEEN,
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

/* function from https://github.com/mcast/json-fileops/blob/jsmn/reHGify/ */
/**
 * Turn a parsed string fragment into a nul-terminated de-escaped C
 * string; in place or copying to a buffer.  Beware, this string could
 * have internal NUL bytes.
 *
 * @param src	The input JSON buffer.
 * @param t	Parsed token, for which we want text.
 * @param dst	Destination buffer; NULL to overwrite in-place at (src + t->start).
 * @param dstn	Size of dst; ignored in destructive mode.
 * @return Length of resulting string excluding terminating NUL, in bytes.
 *
 * If dst[dstn] is not big enough, an incomplete NUL-terminated string
 * is written.
 *
 * If there are somehow character escaping errors in the src[] text
 * which the parser allowed through, they will be passed on unchanged.
 *
 * It should decode wide characters to UTF8, but currently only
 * decodes single-byte \uNNNN values and passes wide characters
 * unchanged.
 */
static size_t
jsmn_nstr (char *src, jsmntok_t *t, char *dst, size_t dstn)
{
	char *src_end = src + t->end;
	char *dst_start, *dst_end;
	src += t->start;

	if (!dst) {
		/* overwrite in place */
		dst = src;
		dstn = t->end - t->start + 1;
	}
	dst_end = dst + dstn - 1;
	dst_start = dst;

	for ( ; src < src_end && dst < dst_end; src++, dst++) {
		if (*src == '\\') {
			src++;
			switch (*src) {
				int chr; // signed; neg = fail
				int i;
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' :
					*dst = *src;
					break;
				case 'b' : *dst = '\b'; break;
				case 'f' : *dst = '\f'; break;
				case 'r' : *dst = '\r'; break;
				case 'n' : *dst = '\n'; break;
				case 't' : *dst = '\t'; break;

				/* Allows escaped symbol \uXXXX */
				case 'u':
					src++;
					chr = 0;
					for (i = 0; i < 4 && src + i < src_end; i++) {
						int val = 0;
						/* If it isn't a hex character we have an error */
						if (src[i] >= 48 && src[i] <= 57) val = src[i] - '0'; /* 0-9 */
						else if (src[i] >= 65 && src[i] <= 70) val = src[i] - 55; /* A-F */
						else if (src[i] >= 97 && src[i] <= 102) val = src[i] - 87; /* a-f */
						else {
							/* Bad, but the parser passed it. Pass through. */
							chr = -1;
							break;
						}
						chr = (chr << 4) + val;
					}
					if (chr < 0) {
						/* Fail */
						*dst = '\\';
						src-=2; /* 'u' again */
						continue;
					} else if (chr < 256) {
						/* Plain char */
						src += 3; /* 4 hex digits, one covered by loop incr */
						*dst = chr;
					} else {
						/* Wide character */
						/* UTF8 not yet implemented. Pretend we didn't see it. */
						*dst = '\\';
						src-=2; /* 'u' again */
						continue;
					}
					break;
				default:
					/* Well the parser already passed on it, so ...? */
					*dst = '\\';
					src --;
			}
		} else {
			*dst = *src;
		}
	}
	*dst = 0;
	return dst - dst_start;
}

static int
json_str_cpy (char *json, jsmntok_t *tok, char **t)
{
	int s_len = tok->end - tok->start;
	*t = calloc(s_len + 1, 1);
	if (*t == NULL) return -1;
	jsmn_nstr(json, tok, *t, s_len + 1);
	return 0;
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
	case STATE_MODULE_REPLACE_BETWEEN:
		state_name = "STATE_MODULE_REPLACE_BETWEEN";
		break;
	case STATE_MODULE_REPLACE:
		state_name = "STATE_MODULE_REPLACE";
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

static int
bad_token (int i, jsmntok_t *tok, char *text, state_t state,
	char *e_text, size_t e_size)
{
	unsigned line = 1;
	unsigned pos_on_line = 1;
	int res;
	int type = tok->type;
	unsigned start_pos = tok->start;
	for (unsigned j = 0; j < start_pos; j++) {
		pos_on_line++;
		if (text[j] == '\n') {
			line++;
			pos_on_line = 1;
		}
	}
	char *str;
	if (tok->end > 0) {
		if (json_str_cpy(text, tok, &str)) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}

		res = return_error(e_text, e_size, TOP_E_TOKEN,
			": unexpected %s token #%d at line %d character %d state %s: %s",
			token_type_name(type), i, line, pos_on_line, state_name(state),
			str);
		free(str);
	} else {
		res = return_error(e_text, e_size, TOP_E_TOKEN,
			": unexpected %s token #%d at line %d character %d state %s",
			token_type_name(type), i, line, pos_on_line, state_name(state));
	}
	return res;
}

static int
parse_submodule_params (submodule_plain_t *submodule, jsmntok_t *tokens,
	char *text, int *i, char *e_text, size_t e_size)
{
	int state = STATE_MODULE_SUBMODULE_PARAMS;
	int subarr_n = tokens[*i].size;

	if (submodule->params != NULL)
		return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
	submodule->params = calloc(subarr_n, sizeof(raw_param_t));
	if (!submodule->params) {
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	}
	submodule->n_params = subarr_n;
	for (int subarr_i = 0; subarr_i < subarr_n; subarr_i++) {
		*i += 1;
		if ((tokens[*i].type != JSMN_OBJECT) ||
			(tokens[*i].size != 1) ||
			(tokens[*i + 1].type != JSMN_STRING) ||
			(tokens[*i + 2].type != JSMN_STRING))
		{
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
		if (json_str_cpy(text, &tokens[*i + 1],
			&submodule->params[subarr_i].name))
		{
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		if (json_str_cpy(text, &tokens[*i + 2],
			&submodule->params[subarr_i].value))
		{
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		*i += 2;
	}

	return 0;
}

static int
parse_submodule (int subobj_n, jsmntok_t *tokens, char *text,
	submodule_wrapper_t *submodule, int *i,
	char *e_text, size_t e_size)
{
	int subres;
	/* at the start, i points at the outermost object */
	int state = STATE_MODULE_SUBMODULE;
	if ((subobj_n == 1) || ((subobj_n == 2) &&
		(json_str_eq(text, &tokens[*i + 1], "rooted") ||
		json_str_eq(text, &tokens[*i + 1], "root"))))
	{
		int subobj_i = 0;
		*i += 1;
		submodule->type = SUBM_HAS_PROD;
		submodule->ptr.prod = calloc(1, sizeof(submodule_prod_t));
		if (!submodule->ptr.prod) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		if (json_str_eq(text, &tokens[*i], "cartesian")) {
			submodule->ptr.prod->type = PROD_IS_CART;
		} else if (json_str_eq(text, &tokens[*i], "tensor")) {
			submodule->ptr.prod->type = PROD_IS_TENS;
		} else if (json_str_eq(text, &tokens[*i], "lexicographical")) {
			submodule->ptr.prod->type = PROD_IS_LEX;
		} else if (json_str_eq(text, &tokens[*i], "strong")) {
			submodule->ptr.prod->type = PROD_IS_STRONG;
		} else if (json_str_eq(text, &tokens[*i], "root")) {
			submodule->ptr.prod->type = PROD_IS_ROOT;
		} else if (json_str_eq(text, &tokens[*i], "rooted")) {
			submodule->ptr.prod->type = PROD_IS_ROOT;
		} else {
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
		if (json_str_eq(text, &tokens[*i], "root")) {
			if (tokens[*i + 1].type != JSMN_STRING) {
				return bad_token(*i + 1, &tokens[*i + 1], text,
					state, e_text, e_size);
			}
			if (json_str_cpy(text, &tokens[*i + 1],
				&submodule->ptr.prod->root))
			{
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			}
			*i += 2;
			if (!json_str_eq(text, &tokens[*i], "rooted")) {
				return bad_token(*i, &tokens[*i], text,
					state, e_text, e_size);
			}
			subobj_i++;
		}

		*i += 1;
		if ((tokens[*i].type != JSMN_ARRAY) ||
			(tokens[*i].size != 2))
		{
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
		submodule->ptr.prod->a = calloc(1, sizeof(submodule_wrapper_t));
		if (!submodule->ptr.prod->a) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		submodule->ptr.prod->b = calloc(1, sizeof(submodule_wrapper_t));
		if (!submodule->ptr.prod->b) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}

		*i += 1;
		if (tokens[*i].type != JSMN_OBJECT) {
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
		if ((subres = parse_submodule(tokens[*i].size, tokens, text,
			submodule->ptr.prod->a, i,
			e_text, e_size)))
		{
			return subres;
		}
		if (tokens[*i].type != JSMN_OBJECT) {
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
		if ((subres = parse_submodule(tokens[*i].size, tokens, text,
			submodule->ptr.prod->b, i,
			e_text, e_size)))
		{
			return subres;
		}
		if ((submodule->ptr.prod->type == PROD_IS_ROOT) &&
			(subobj_i == 0))
		{
			if ((!json_str_eq(text, &tokens[*i], "root"))
				|| (tokens[*i + 1].type != JSMN_STRING))
			{
				return bad_token(*i, &tokens[*i], text,
					state, e_text, e_size);
			}
			if (json_str_cpy(text, &tokens[*i + 1],
				&submodule->ptr.prod->root))
			{
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			}
			*i += 2;
		}

	} else if (((subobj_n == 2) || (subobj_n == 3)) &&
		((json_str_eq(text, &tokens[*i + 1], "if")) ||
		(json_str_eq(text, &tokens[*i + 1], "then")) ||
		(json_str_eq(text, &tokens[*i + 1], "else"))))
	{
		*i += 1;
		submodule->type = SUBM_HAS_COND;
		submodule->ptr.cond = calloc(1, sizeof(submodule_cond_t));
		if (!submodule->ptr.cond)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "if")) {
				if (submodule->ptr.cond->condition != NULL)
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&submodule->ptr.cond->condition))
				{
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "then")) {
				*i += 1;
				if ((tokens[*i].type != JSMN_OBJECT) ||
					(submodule->ptr.cond->subm_then != NULL))
				{
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				submodule->ptr.cond->subm_then =
					calloc(1, sizeof(submodule_wrapper_t));
				if (!submodule->ptr.cond->subm_then)
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				if ((subres = parse_submodule(tokens[*i].size,
					tokens, text,
					submodule->ptr.cond->subm_then, i,
					e_text, e_size)) != 0)
				{
					return subres;
				}
			} else if (json_str_eq(text, &tokens[*i], "else")) {
				if (!submodule->ptr.cond->subm_then)
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				*i += 1;
				if ((tokens[*i].type != JSMN_OBJECT) ||
					(submodule->ptr.cond->subm_else != NULL))
				{
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				submodule->ptr.cond->subm_else =
					calloc(1, sizeof(submodule_wrapper_t));
				if (!submodule->ptr.cond->subm_else)
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				if ((subres = parse_submodule(tokens[*i].size,
					tokens, text,
					submodule->ptr.cond->subm_else, i,
					e_text, e_size)) != 0)
				{
					return subres;
				}
			} else {
				return bad_token(*i, &tokens[*i], text, state,
					e_text, e_size);
			}
		}
	} else {
		*i += 1;
		submodule->type = SUBM_HAS_SUBM;
		submodule->ptr.subm = calloc(1, sizeof(submodule_plain_t));
		if (!submodule->ptr.subm) {
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		}
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "name")) {
				if (submodule->ptr.subm->name != NULL) {
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[*i + 1],
					&submodule->ptr.subm->name))
				{
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "module")) {
				if (submodule->ptr.subm->module != NULL) {
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[*i + 1],
					&submodule->ptr.subm->module))
				{
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "size")) {
				if (submodule->ptr.subm->size != NULL) {
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[*i + 1],
					&submodule->ptr.subm->size))
				{
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "params")) {
				*i += 1;
				if (tokens[*i].type != JSMN_ARRAY) {
					return bad_token(*i, &tokens[*i], text,
						state, e_text, e_size);
				}
				if ((subres = parse_submodule_params(submodule->ptr.subm,
					tokens, text, i, e_text, e_size)))
				{
					return subres;
				}
				*i += 1;
			} else {
				return bad_token(*i, &tokens[*i], text, state,
					e_text, e_size);
			}
		}
	}
	
	return 0;
}

static int
parse_replace (int subobj_n, jsmntok_t *tokens, char *text,
	replace_t *replace, int *i,
	char *e_text, size_t e_size)
{
	int subres;
	/* at the start, i points at the outermost object */
	int state = STATE_MODULE_REPLACE;
	if (subobj_n != 2) {
		return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
	}
	*i += 1;
	for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
		if (json_str_eq(text, &tokens[*i], "nodes")) {
			if (replace->nodes != NULL)
				return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
			if (json_str_cpy(text, &tokens[*i + 1],
				&replace->nodes))
			{
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			}
			*i += 2;
		} else if (json_str_eq(text, &tokens[*i], "with")) {
			*i += 1;
			if ((tokens[*i].type != JSMN_OBJECT) ||
				(replace->submodule != NULL))
			{
				return bad_token(*i, &tokens[*i], text,
					state, e_text, e_size);
			}
			replace->submodule = calloc(1, sizeof(submodule_wrapper_t));
			if (!replace->submodule)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			if ((subres = parse_submodule(tokens[*i].size, tokens, text,
				replace->submodule, i, e_text, e_size)) != 0)
			{
				return subres;
			}
		} else {
			return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
		}
	}
	return 0;
}


static int
parse_connection (int subobj_n, jsmntok_t *tokens, char *text,
	connection_wrapper_t *connection, int *i,
	char *e_text, size_t e_size)
{
	int subres;
	/* at the start, i points at the outermost object */
	int state = STATE_MODULE_CONNECTION;
	if (subobj_n > 5) {
		return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
	}

	*i += 1;
	if (((subobj_n == 2) || (subobj_n == 3)) &&
		(json_str_eq(text, &tokens[*i], "attributes") ||
		json_str_eq(text, &tokens[*i], "from") ||
		json_str_eq(text, &tokens[*i], "to")))
	{
		connection->type = CONN_HAS_CONN;
		connection->ptr.conn = calloc(1, sizeof(connection_plain_t));
		if (!connection->ptr.conn)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "from")) {
				if (connection->ptr.conn->from != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.conn->from))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "to")) {
				if (connection->ptr.conn->to != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.conn->to))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "attributes")) {
				if (connection->ptr.conn->attributes != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.conn->attributes))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				*i += 2;
			} else {
				return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
			}
		}
	} else if (((subobj_n == 2) || (subobj_n == 3)) &&
		(json_str_eq(text, &tokens[*i], "if") ||
		json_str_eq(text, &tokens[*i], "then") ||
		json_str_eq(text, &tokens[*i], "else")))
	{
		connection->type = CONN_HAS_COND;
		connection->ptr.cond = calloc(1, sizeof(connection_cond_t));
		if (!connection->ptr.cond)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "if")) {
				if (connection->ptr.cond->condition != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.cond->condition))
				{
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "then")) {
				*i += 1;
				if ((tokens[*i].type != JSMN_OBJECT) ||
					(connection->ptr.cond->conn_then != NULL))
				{
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
				connection->ptr.cond->conn_then =
					calloc(1, sizeof(connection_wrapper_t));
				if (!connection->ptr.cond->conn_then)
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				if ((subres = parse_connection(tokens[*i].size,
					tokens, text,
					connection->ptr.cond->conn_then, i,
					e_text, e_size)) != 0)
				{
					return subres;
				}
			} else if (json_str_eq(text, &tokens[*i], "else")) {
				if (connection->ptr.cond->conn_then == NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				*i += 1;
				if ((tokens[*i].type != JSMN_OBJECT) ||
					(connection->ptr.cond->conn_else != NULL))
				{
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
				connection->ptr.cond->conn_else =
					calloc(1, sizeof(connection_wrapper_t));
				if (!connection->ptr.cond->conn_else)
					return return_error(e_text, e_size,
						TOP_E_ALLOC, "");
				if ((subres = parse_connection(tokens[*i].size,
					tokens, text,
					connection->ptr.cond->conn_else, i,
					e_text, e_size)) != 0)
				{
					return subres;
				}
			} else {
				return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
			}
		}
	} else if ((subobj_n == 4) || (subobj_n == 5)) {
		connection->type = CONN_HAS_LOOP;
		for (int t = 0; t < subobj_n; t++) {
			if (tokens[*i + 1].type == JSMN_OBJECT)
				break;
			if (json_str_eq(text, &tokens[*i], "ring")) {
				connection->type = CONN_HAS_RING;
				break;
			}
			if (json_str_eq(text, &tokens[*i], "line")) {
				connection->type = CONN_HAS_LINE;
				break;
			}
			if (json_str_eq(text, &tokens[*i], "all")) {
				connection->type = CONN_HAS_ALLLIST;
				break;
			}
		}
		if (connection->type == CONN_HAS_LOOP) {
			connection->ptr.loop = calloc(1, sizeof(connection_loop_t));
			if (!connection->ptr.loop)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
				if (json_str_eq(text, &tokens[*i], "start")) {
					if (connection->ptr.loop->start != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.loop->start))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "end")) {
					if (connection->ptr.loop->end != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.loop->end))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "loop")) {
					if (connection->ptr.loop->loop != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.loop->loop))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "conn")) {
					*i += 1;
					if ((tokens[*i].type != JSMN_OBJECT) ||
						(connection->ptr.loop->conn != NULL))
					{
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					}
					connection->ptr.loop->conn =
						calloc(1, sizeof(connection_wrapper_t));
					if (!connection->ptr.loop->conn)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					if ((subres = parse_connection(tokens[*i].size,
						tokens, text,
						connection->ptr.loop->conn, i,
						e_text, e_size)) != 0)
					{
						return subres;
					}

				} else {
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
			}
		} else if (connection->type == CONN_HAS_ALLLIST) {
			connection->ptr.alllist = calloc(1, sizeof(connection_ring_t));
			if (!connection->ptr.alllist)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
				if (json_str_eq(text, &tokens[*i], "start")) {
					if (connection->ptr.alllist->start != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.alllist->start))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "end")) {
					if (connection->ptr.alllist->end != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.alllist->end))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "all")) {
					if (connection->ptr.alllist->var != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.alllist->var))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "conn")) {
					if (connection->ptr.alllist->nodes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.alllist->nodes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "attributes")) {
					if (connection->ptr.alllist->attributes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.alllist->attributes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else {
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
			}
		} else if (connection->type == CONN_HAS_RING) {
			connection->ptr.ring = calloc(1, sizeof(connection_ring_t));
			if (!connection->ptr.ring)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
				if (json_str_eq(text, &tokens[*i], "start")) {
					if (connection->ptr.ring->start != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.ring->start))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "end")) {
					if (connection->ptr.ring->end != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.ring->end))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "ring")) {
					if (connection->ptr.ring->var != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.ring->var))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "conn")) {
					if (connection->ptr.ring->nodes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.ring->nodes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "attributes")) {
					if (connection->ptr.ring->attributes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.ring->attributes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else {
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
			}
		} else if (connection->type == CONN_HAS_LINE) {
			connection->ptr.line = calloc(1, sizeof(connection_line_t));
			if (!connection->ptr.line)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
			for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
				if (json_str_eq(text, &tokens[*i], "start")) {
					if (connection->ptr.line->start != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.line->start))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "end")) {
					if (connection->ptr.line->end != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.line->end))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "line")) {
					if (connection->ptr.line->var != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.line->var))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "conn")) {
					if (connection->ptr.line->nodes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.line->nodes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else if (json_str_eq(text, &tokens[*i], "attributes")) {
					if (connection->ptr.line->attributes != NULL)
						return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
					if (json_str_cpy(text, &tokens[*i + 1],
						&connection->ptr.line->attributes))
					{
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
					}
					*i += 2;
				} else {
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				}
			}
		}
	} else if (((subobj_n == 1) || (subobj_n == 2)) &&
		(json_str_eq(text, &tokens[*i], "all-match") ||
		(json_str_eq(text, &tokens[*i], "attributes") &&
		json_str_eq(text, &tokens[*i], "all-match")))) 
	{
		connection->type = CONN_HAS_ALL;
		connection->ptr.all = calloc(1, sizeof(connection_all_t));
		if (!connection->ptr.all)
			return return_error(e_text, e_size, TOP_E_ALLOC, "");
		for (int subobj_i = 0; subobj_i < subobj_n; subobj_i++) {
			if (json_str_eq(text, &tokens[*i], "all-match")) {
				if (connection->ptr.all->nodes != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.all->nodes))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				*i += 2;
			} else if (json_str_eq(text, &tokens[*i], "attributes")) {
				if (connection->ptr.all->attributes != NULL)
					return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
				if (json_str_cpy(text, &tokens[*i + 1],
					&connection->ptr.all->attributes))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				*i += 2;
			} else {
				return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
			}
		}
	} else {
		return bad_token(*i, &tokens[*i], text, state, e_text, e_size);
	}
	return 0;
}

static int
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
	network_definition_t *net, char *e_text, size_t e_size)
{
	int res = 0;
	int subres;
	module_t *m = NULL;
	network_t *network = NULL;
	int m_n = 0;
	int m_i = -1;
	int i = 0;
	int arr_i = 0;
	int arr_n = 0;
	int obj_i = 0;
	int obj_n = 0;
	int subobj_i = 0;
	int subobj_n = 0;
	state_t state = STATE_START;

	do {
		if (state == STATE_START) {
			if (tokens[i].type != JSMN_ARRAY) {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			}
			m_n = tokens[i].size;
			m_i = net->n_modules - 1;
			m = realloc(net->modules,
				(net->n_modules + m_n) * sizeof(module_t));
			if (!m)
				return return_error(e_text, e_size, TOP_E_ALLOC, "");
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
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			}
			if (json_str_eq(text, &tokens[i + 1], "module") ||
				json_str_eq(text, &tokens[i + 1], "simplemodule"))
			{
				m_i += 1;
				if (json_str_eq(text, &tokens[i + 1], "module")) {
					m[m_i].type = MODULE_COMPOUND;
				} else {
					m[m_i].type = MODULE_SIMPLE;
				}
				state = STATE_MODULE;
				i += 2;
				if (tokens[i].type != JSMN_OBJECT)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
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
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				}
				network = calloc(1, sizeof(network_t));
				if (!network)
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				net->network = network;
				obj_n = tokens[i].size;
				obj_i = 0;
				i += 1;
			} else {
				return bad_token(i + 1, &tokens[i + 1], text, state, e_text, e_size);
			}

		} else if (state == STATE_MODULE) {
			if (obj_i == obj_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "name")) {
				if ((m[m_i].name != NULL) ||
					(tokens[i + 1].type != JSMN_STRING))
				{
					return bad_token(i + 1, &tokens[i + 1],
						text, state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&m[m_i].name))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				obj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "attributes") &&
				(m[m_i].type == MODULE_SIMPLE))
			{
				if ((m[m_i].attributes != NULL) ||
					(tokens[i + 1].type != JSMN_STRING))
				{
					return bad_token(i + 1, &tokens[i + 1],
						text, state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&m[m_i].attributes))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				obj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				state = STATE_MODULE_PARAMS;
			} else if (json_str_eq(text, &tokens[i], "submodules")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (m[m_i].submodules != NULL)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				if (arr_n > 0) {
					m[m_i].submodules =
						calloc(arr_n, sizeof(submodule_wrapper_t));
					if (!m[m_i].submodules)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				m[m_i].n_submodules = arr_n;
				i += 1;
				state = STATE_MODULE_SUBMODULES_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "replace")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (m[m_i].replace != NULL)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				if (arr_n > 0) {
					m[m_i].replace =
						calloc(arr_n, sizeof(replace_t));
					if (!m[m_i].replace)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				m[m_i].n_replace = arr_n;
				i += 1;
				state = STATE_MODULE_REPLACE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "connections")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				if (m[m_i].connections != NULL)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (arr_n > 0) {
					m[m_i].connections =
						calloc(arr_n,
						sizeof(connection_wrapper_t));
					if (!m[m_i].connections)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				m[m_i].n_connections = arr_n;
				i += 1;
				state = STATE_MODULE_CONNECTIONS_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "gates")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				if (m[m_i].gates != NULL)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				arr_n = tokens[i].size;
				arr_i = 0;
				if (arr_n > 0) {
					m[m_i].gates =
						calloc(arr_n, sizeof(gate_t));
					if (!m[m_i].gates)
						return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				m[m_i].n_gates = arr_n;
				i += 1;
				state = STATE_MODULE_GATES_BETWEEN;
			} else {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			}

		} else if (state == STATE_MODULE_PARAMS) {
			arr_n = tokens[i].size;
			if (m[m_i].params != NULL)
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			if (arr_n > 0) {
				m[m_i].params =
					calloc(arr_n, sizeof(raw_param_t));
				if (!m[m_i].params)
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
			}
			m[m_i].n_params = arr_n;
			i += 1;
			for (arr_i = 0; arr_i < arr_n; arr_i++) {
				if ((tokens[i].type != JSMN_OBJECT) ||
					(tokens[i].size != 1) ||
					(tokens[i + 1].type != JSMN_STRING) ||
					(tokens[i + 2].type != JSMN_STRING))
				{
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&m[m_i].params[arr_i].name))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				if (json_str_cpy(text, &tokens[i + 2],
					&m[m_i].params[arr_i].value))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				i += 3;
			}
			state = STATE_MODULE;

		} else if (state == STATE_MODULE_GATES_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
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
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			} else {
				state = STATE_MODULE_CONNECTION;
			}

		} else if (state == STATE_MODULE_SUBMODULES_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			} else {
				state = STATE_MODULE_SUBMODULE;
			}

		} else if (state == STATE_MODULE_REPLACE_BETWEEN) {
			if (arr_i == arr_n) {
				state = STATE_MODULE;
			} else if (tokens[i].type != JSMN_OBJECT) {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			} else {
				state = STATE_MODULE_REPLACE;
			}

		} else if (state == STATE_MODULE_CONNECTION) {
			subobj_i = 0;
			subobj_n = tokens[i].size;
			if ((subres = parse_connection(subobj_n, tokens, text,
				&m[m_i].connections[arr_i], &i, e_text,
				e_size)) != 0)
			{
				return subres;
			}
			state = STATE_MODULE_CONNECTIONS_BETWEEN;
			arr_i += 1;

		} else if (state == STATE_MODULE_SUBMODULE) {
			subobj_i = 0;
			subobj_n = tokens[i].size;
			if ((subres = parse_submodule(subobj_n, tokens, text,
				&m[m_i].submodules[arr_i], &i, e_text,
				e_size)) != 0)
			{
				return subres;
			}
			state = STATE_MODULE_SUBMODULES_BETWEEN;
			arr_i += 1;

		} else if (state == STATE_MODULE_REPLACE) {
			subobj_i = 0;
			subobj_n = tokens[i].size;
			if ((subres = parse_replace(subobj_n, tokens, text,
				&m[m_i].replace[arr_i], &i, e_text,
				e_size)) != 0)
			{
				return subres;
			}
			state = STATE_MODULE_REPLACE_BETWEEN;
			arr_i += 1;

		} else if (state == STATE_MODULE_GATE) {
			if (subobj_i == subobj_n) {
				state = STATE_MODULE_GATES_BETWEEN;
				arr_i += 1;
			} else {
				if (json_str_cpy(text, &tokens[i],
					&m[m_i].gates[arr_i].name))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&m[m_i].gates[arr_i].size))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				i += 2;
				subobj_i += 1;
			}

		} else if (state == STATE_NETWORK) {
			if (obj_i == obj_n) {
				state = STATE_BETWEEN;
			} else if (json_str_eq(text, &tokens[i], "module")) {
				if (network->module != NULL) {
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&network->module))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				obj_i += 1;
				i += 2;
			} else if (json_str_eq(text, &tokens[i], "params")) {
				obj_i += 1;
				i += 1;
				if (tokens[i].type != JSMN_ARRAY)
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				state = STATE_NETWORK_PARAMS;
			} else {
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			}

		} else if (state == STATE_NETWORK_PARAMS) {
			arr_n = tokens[i].size;
			if (network->params != NULL)
				return bad_token(i, &tokens[i], text, state, e_text, e_size);
			if (arr_n > 0) {
				network->params = calloc(arr_n, sizeof(raw_param_t));
				if (!network->params)
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
			}
			network->n_params = arr_n;
			i += 1;
			for (arr_i = 0; arr_i < arr_n; arr_i++) {
				if ((tokens[i].type != JSMN_OBJECT) ||
					(tokens[i].size != 1) ||
					(tokens[i + 1].type != JSMN_STRING) ||
					(tokens[i + 2].type != JSMN_STRING))
				{
					return bad_token(i, &tokens[i], text, state, e_text, e_size);
				}
				if (json_str_cpy(text, &tokens[i + 1],
					&network->params[arr_i].name))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				if (json_str_cpy(text, &tokens[i + 2],
					&network->params[arr_i].value))
				{
					return return_error(e_text, e_size, TOP_E_ALLOC, "");
				}
				i += 3;
			}
			state = STATE_NETWORK;
		}
	} while (i < n_tokens);

	return res;
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
	network_definition_t *net, char *e_text, size_t e_size)
{

	jsmntok_t *tokens;
	jsmn_parser parser;
	jsmn_init(&parser);
	int n_tokens = jsmn_parse(&parser, text, file_size, NULL, 0);
	if (n_tokens < 0)
		return return_error(e_text, e_size, TOP_E_JSON, "");
	tokens = malloc(sizeof(jsmntok_t) * (size_t) n_tokens);
	if (tokens == NULL)
		return return_error(e_text, e_size, TOP_E_ALLOC, "");
	jsmn_init(&parser);
	jsmn_parse(&parser, text, file_size, tokens, n_tokens);
	/* json_print(tokens, n_tokens, text); */

	int res = json_deserialize(tokens, n_tokens, text, net, e_text, e_size);
	free(tokens);
	return res;
}
