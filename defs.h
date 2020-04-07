#ifndef DEFS_H
# define DEFS_H

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

enum { GRAPH_BLK_SIZE = 32 };

typedef struct stack stack_t;

struct stack {
	char *name;
	stack_t *next;
};

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
	int n_submodules;
	//gate_t *gates;
	char **gates;
	char **connections;
	int n_connections;
} module_t;

typedef struct {
	char *module;
	char **params;
} network_t;

typedef struct {
	module_t *modules;
	network_t *network;
	int n_modules;
} network_definition_t;

#endif
