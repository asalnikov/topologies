#ifndef PRODUCTS_H
# define PRODUCTS_H

int
graphs_cart_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size);

int
graphs_tens_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size);

int
graphs_lex_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size);

int
graphs_strong_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *e_text, size_t e_size);

int
graphs_root_product (graph_t *g_a, graph_t *g_b, graph_t *g_prod,
	char *root, char *e_text, size_t e_size);

#endif
