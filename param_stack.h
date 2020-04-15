#ifndef PARAM_STACK_H
# define PARAM_STACK_H

param_stack_t *
param_stack_create (void);

void
param_stack_leave (param_stack_t *p);

int
param_stack_to_te_vars (param_stack_t *p, te_variable **vars);

int
param_stack_eval (param_stack_t *p, char *value, double *rval);

int
param_stack_enter (param_stack_t *p, raw_param_t *r, char *e_text,
	size_t e_size);

int
param_stack_enter_val (param_stack_t *p, char *name, int d);

void
param_stack_destroy (param_stack_t *p);

/* void
param_stack_print (param_stack_t *p, FILE *stream); */

int
eval_conn_name (param_stack_t *p, char *name, char **r_name,
	char *e_text, size_t e_size);

#endif
