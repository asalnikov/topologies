#ifndef DEFS_H
# define DEFS_H

/* graph */

typedef enum {
	NODE_NODE,
	NODE_GATE,
	NODE_GATE_VISITED,
	NODE_REPLACED_T,
	NODE_REPLACED
} node_type;

typedef struct {
	int n;
	char *attributes;
} edge_t;

typedef struct {
	char *name;
	int n;
	edge_t *adj;
	int n_adj;
	int cap_adj;
	node_type type;
	char *attributes;
} node_t;

typedef struct {
	node_t *nodes;
	int n_nodes;
	int cap_nodes;
} graph_t;

enum { GRAPH_BLK_SIZE = 32 };
enum { ADJ_BLK_SIZE = 8 };

/* name stack */

typedef struct name_stack name_stack_t;

struct name_stack {
	char *name;
	name_stack_t *next;
};

/* param stack */

enum { PARAM_BLK_SIZE = 32 };

typedef struct param {
	char *name;
	double value;
} param_t;

typedef struct param_stack {
	param_t *params;
	int n;
	int cap;
} param_stack_t;

/* network representation */

typedef struct {
	char *name;
	char *value;
} raw_param_t;

typedef struct connection_wrapper connection_wrapper_t;

typedef enum {
	CONN_HAS_CONN,
	CONN_HAS_LOOP,
	CONN_HAS_COND,
	CONN_HAS_ALLLIST,
	CONN_HAS_ALL,
	CONN_HAS_LINE,
	CONN_HAS_RING
} connection_type_t;

typedef struct {
	char *condition;
	connection_wrapper_t *conn_then;
	connection_wrapper_t *conn_else;
} connection_cond_t;

typedef struct {
	char *loop;
	char *start;
	char *end;
	connection_wrapper_t *conn;
} connection_loop_t;

typedef struct {
	char *nodes;
	char *attributes;
} connection_all_t;

typedef struct {
	char *var;
	char *start;
	char *end;
	char *nodes;
	char *attributes;
} connection_ring_t;

typedef struct {
	char *var;
	char *start;
	char *end;
	char *nodes;
	char *attributes;
} connection_line_t;

typedef struct {
	char *var;
	char *start;
	char *end;
	char *nodes;
	char *attributes;
} connection_alllist_t;

typedef struct {
	char *from;
	char *to;
	char *attributes;
} connection_plain_t;

struct connection_wrapper {
	connection_type_t type;
	union {
		connection_plain_t *conn;
		connection_loop_t *loop;
		connection_cond_t *cond;
		connection_all_t *all;
		connection_alllist_t *alllist;
		connection_line_t *line;
		connection_ring_t *ring;
	} ptr;
};

typedef struct submodule_wrapper submodule_wrapper_t;

typedef enum {
	SUBM_HAS_SUBM,
	SUBM_HAS_PROD,
	SUBM_HAS_COND
} submodule_type_t;

typedef struct {
	char *name;
	char *module;
	char *size;
	raw_param_t *params;
	int n_params;
} submodule_plain_t;

typedef enum {
	PROD_IS_CART,
	PROD_IS_TENS,
	PROD_IS_LEX,
	PROD_IS_STRONG,
	PROD_IS_ROOT
} prod_type_t;

typedef struct {
	submodule_wrapper_t *a;
	submodule_wrapper_t *b;
	prod_type_t type;
	char *root;
} submodule_prod_t;

typedef struct {
	char *condition;
	submodule_wrapper_t *subm_then;
	submodule_wrapper_t *subm_else;
} submodule_cond_t;

struct submodule_wrapper {
	submodule_type_t type;
	union {
		submodule_plain_t *subm;
		submodule_prod_t *prod;
		submodule_cond_t *cond;
	} ptr;
};

typedef struct {
	char *name;
	char *size;
} gate_t;

typedef enum {
	MODULE_COMPOUND,
	MODULE_SIMPLE
} module_type_t;

typedef struct {
	char *nodes;
	submodule_wrapper_t *submodule;
} replace_t;

typedef struct {
	char *name;
	raw_param_t *params;
	int n_params;
	submodule_wrapper_t *submodules;
	int n_submodules;
	gate_t *gates;
	int n_gates;
	connection_wrapper_t *connections;
	int n_connections;
	module_type_t type;
	char *attributes;
	replace_t *replace;
	int n_replace;
} module_t;

typedef struct {
	char *module;
	raw_param_t *params;
	int n_params;
} network_t;

typedef struct {
	module_t *modules;
	network_t *network;
	int n_modules;
} network_definition_t;

#endif
