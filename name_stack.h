#ifndef NAME_STACK_H
# define NAME_STACK_H

name_stack_t *
name_stack_create (char *name);

void
name_stack_enter (name_stack_t *s, char *name, int index);

void
name_stack_leave (name_stack_t *s);

char *
name_stack_name (name_stack_t *s);

char *
get_full_name (name_stack_t *s, char *name, int index);

#endif
