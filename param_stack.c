#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "tinyexpr.h"

#include "parser.h"
#include "param_stack.h"
#include "defs.h"

param_stack_t *
param_stack_create (void)
{
	param_stack_t *p = (param_stack_t *) malloc(sizeof(param_stack_t));
	p->n = 0;
	p->cap = PARAM_BLK_SIZE;
	p->params = (param_t *) calloc(p->cap, sizeof(param_t));
	return p;
}

void
param_stack_leave (param_stack_t *p)
{
	p->n--;
}

int
param_stack_to_te_vars (param_stack_t *p, te_variable **vars)
{
	int VARS_BLK_SIZE = 32;
	int te_vars_n = 0;
	int te_vars_cap = VARS_BLK_SIZE;
	*vars = (te_variable *) calloc(te_vars_cap, sizeof(te_variable));

	/* add starting from the tail -- from the most recent */
	for (int i = p->n - 1; i >= 0; i--) {
		bool is_present = false;
		int param_name_len = strlen(p->params[i].name);
		for (int j = 0; j < te_vars_n; j++) {
			if (strncmp(p->params[i].name, (*vars)[j].name,
				param_name_len) == 0)
			{
				is_present = true;
				break;
			}
		}
		if (is_present) {
			continue;
		}
		if (te_vars_n == te_vars_cap) {
			te_vars_cap += VARS_BLK_SIZE;
			*vars = (te_variable *) realloc(*vars,
				te_vars_cap * sizeof(te_variable));
		}
		(*vars)[te_vars_n].name = p->params[i].name;
		(*vars)[te_vars_n].address = &p->params[i].value;
		te_vars_n++;
	}

	return te_vars_n;
}

int
param_stack_eval (param_stack_t *p, char *value, double *rval)
{
	te_variable *vars = NULL;
	int vars_n = param_stack_to_te_vars(p, &vars);
	int err;
	te_expr *e = te_compile(value, vars, vars_n, &err);
	if (e == NULL) {
		return -1;
	}
	*rval = te_eval(e);
	te_free(e);
	free(vars);
	return 0;
}

int
param_stack_enter (param_stack_t *p, raw_param_t *r)
{
	if (p->n == p->cap) {
		p->cap += PARAM_BLK_SIZE;
		p->params = (param_t *) realloc(p->params,
			p->cap * sizeof(param_t));
	}

	/* param stack's lifetime is contained in raw params' lifetime */
	p->params[p->n].name = r->name;
	p->n++;
	return param_stack_eval(p, r->value, &p->params[p->n - 1].value);
}

void
param_stack_enter_val (param_stack_t *p, char *name, int d)
{
	if (p->n == p->cap) {
		p->cap += PARAM_BLK_SIZE;
		p->params = (param_t *) realloc(p->params,
			p->cap * sizeof(param_t));
	}

	/* param stack's lifetime is contained in name's lifetime */
	p->params[p->n].name = name;
	p->params[p->n].value = d;
	p->n++;
}

void
param_stack_destroy (param_stack_t *p)
{
	free(p->params);
	free(p);
}

/* void
param_stack_print (param_stack_t *p, FILE *stream)
{
	for (int i = 0; i < p->n; i++) {
		fprintf(stream, "%s = %f, ", p->params[i].name,
			p->params[i].value);
	}
} */

char *
eval_conn_name (param_stack_t *p, char *name)
{
	int len = strlen(name);
	char *left, *right;

	int n_params = 0;
	left = name;
	while ((left = strchr(left, '[')) != NULL) {
		left++;
		n_params++;
	}

	int *eval_res = calloc(n_params, sizeof(int));
	int i = 0;
	int added_len = 0;
	double tmp_d;

	left = name;
	while ((left = strchr(left, '[')) != NULL) {
		right = strchr(left, ']');
		if (right == NULL) {
			free(eval_res);
			return NULL;
		}
		*right = 0;
		left++;	
		if (param_stack_eval(p, left, &tmp_d) < 0)
			return NULL;
		eval_res[i] = lrint(tmp_d);
		*right = ']';
		added_len += snprintf(0, 0, "%d", eval_res[i]);
		i++;
	}

	char *res_str = calloc(len + added_len + 1, sizeof(char));
	char *t = res_str;
	left = name;
	char *prev = left;
	i = 0;
	while ((left = strchr(left, '[')) != NULL) {
		strncpy(t, prev, left - prev + 1);
		t += left - prev + 1;
		t += snprintf(t, res_str + len + added_len - t, "%d]",
			eval_res[i]);
		left = strchr(left, ']');
		prev = left + 1;
		i++;
	}
	strcpy(t, prev);
	free(eval_res);
	return res_str;
}
