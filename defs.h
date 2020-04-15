#ifndef DEFS_H
# define DEFS_H

/* graph */

typedef struct node_list node_list_t;

typedef enum {
	NODE_NODE,
	NODE_GATE,
	NODE_GATE_VISITED
} node_type;

typedef struct {
	char *name;
	node_list_t *adj;
	int n;
	node_type type;
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

enum { GRAPH_BLK_SIZE = 32 };

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
	CONN_HAS_COND
} connection_type_t;

typedef struct {
	char *condition;
	connection_wrapper_t *conn;
} connection_cond_t;

typedef struct {
	char *loop;
	char *start;
	char *end;
	connection_wrapper_t *conn;
} connection_loop_t;

typedef struct {
	char *from;
	char *to;
} connection_plain_t;

struct connection_wrapper {
	connection_type_t type;
	union {
		connection_plain_t *conn;
		connection_loop_t *loop;
		connection_cond_t *cond;
	} ptr;
};

typedef struct {
	char *name;
	char *size;
} gate_t;

typedef struct {
	char *name;
	char *module;
	char *size;
	raw_param_t *params;
	int n_params;
} submodule_t;

typedef enum {
	MODULE_COMPOUND,
	MODULE_SIMPLE
} module_type_t;

typedef struct {
	char *name;
	raw_param_t *params;
	int n_params;
	submodule_t *submodules;
	int n_submodules;
	gate_t *gates;
	int n_gates;
	connection_wrapper_t *connections;
	int n_connections;
	module_type_t type;
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
