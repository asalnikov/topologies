#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "defs.h"
#include "name_stack.h"

name_stack_t *
name_stack_create (char *name)
{
	name_stack_t *s = (name_stack_t *) malloc(sizeof(name_stack_t));
	s->next = NULL;
	s->name = (char *) malloc(strlen(name) + 1);
	strncpy(s->name, name, strlen(name) + 1);
	return s;
}

void
name_stack_enter (name_stack_t *s, char *name, int index)
{
	int len;
	name_stack_t *head = s;
	while (head->next != NULL)
		head = head->next;
	head->next = (name_stack_t *) malloc(sizeof(name_stack_t));
	head = head->next;
	head->next = NULL;
	if (index < 0) {
		len = strlen(name) + 1;
		head->name = (char *) malloc(len);
		strncpy(s->name, name, len);
	} else {
		len = snprintf(0, 0, "%s[%d]", name, index) + 1;
		head->name = (char *) malloc(len);
		snprintf(head->name, len, "%s[%d]", name, index);
	}
}

void
name_stack_leave (name_stack_t *s)
{
	name_stack_t *head = s;
	if (head->next == NULL)
		return;
	while (head->next->next != NULL)
		head = head->next;
	free(head->next->name);
	free(head->next);
	head->next = NULL;
}

char *
name_stack_name (name_stack_t *s)
{
	unsigned len = 0, off = 0;
	name_stack_t *head = s;
	while (head->next != NULL) {
		len += strlen(head->name) + 1;
		head = head->next;
	}
	len += strlen(head->name) + 1;
	char *name = (char *) malloc(len);
	head = s;
	while (head->next != NULL) {
		snprintf(name + off, strlen(head->name) + 2, "%s.", head->name);
		off += strlen(head->name) + 1;
		head = head->next;
	}
	snprintf(name + off, strlen(head->name) + 2, "%s", head->name);
	return name;
}

char *
get_full_name (name_stack_t *s, char *name, int index)
{
	int added_len = 0;
	if (index != -1) {
		added_len = snprintf(0, 0, "[%d]", index);
	}
	char *name_s = name_stack_name(s);
	char *full_name =
		(char *) malloc(strlen(name) + strlen(name_s) + added_len + 2);
	strncpy(full_name, name_s, strlen(name_s));
	full_name[strlen(name_s)] = '.';
	strncpy(full_name + strlen(name_s) + 1, name,
	        strlen(name) + 1);
	if (index != -1) {
		snprintf(full_name + strlen(name_s) + strlen(name) + 1,
			added_len + 1, "[%d]", index);
	}
	free(name_s);
	return full_name;
}

