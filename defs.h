#ifndef DEFS_H
# define DEFS_H

/* graph */

typedef struct node_list node_list_t;

typedef enum {
	NODE_NODE,
	NODE_GATE
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

typedef struct raw_param {
	char *name;
	char *value;
} raw_param_t;

typedef struct {
	char *name;
	char *module;
	char *size;
	raw_param_t *params;
	int n_params;
} submodule_t;

typedef struct {
	char *name;
	raw_param_t *params;
	int n_params;
	submodule_t *submodules;
	int n_submodules;
	char **gates;
	int n_gates;
	char **connections;
	int n_connections;
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
