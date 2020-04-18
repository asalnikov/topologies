#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "defs.h"
#include "name_stack.h"
#include "errors.h"

name_stack_t *
name_stack_create (char *name)
{
	name_stack_t *s = (name_stack_t *) malloc(sizeof(name_stack_t));
	if (!s) return NULL;
	s->next = NULL;
	s->name = (char *) malloc(strlen(name) + 1);
	if (!s->name) {
		free(s);
		return NULL;
	}
	strncpy(s->name, name, strlen(name) + 1);
	return s;
}

int
name_stack_enter (name_stack_t *s, char *name, int index)
{
	int len;
	name_stack_t *head = s;
	while (head->next != NULL)
		head = head->next;
	head->next = (name_stack_t *) malloc(sizeof(name_stack_t));
	if (!head->next)
		return TOP_E_ALLOC;
	head = head->next;
	head->next = NULL;
	if (index < 0) {
		len = strlen(name) + 1;
		head->name = (char *) malloc(len);
		if (!head->name) {
			free(head);
			return TOP_E_ALLOC;
		}
		strncpy(head->name, name, len);
	} else {
		len = snprintf(0, 0, "%s[%d]", name, index) + 1;
		head->name = (char *) malloc(len);
		if (!head->name) {
			free(head);
			return TOP_E_ALLOC;
		}
		snprintf(head->name, len, "%s[%d]", name, index);
	}
	return 0;
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
	if (!head) return NULL;
	while (head->next != NULL) {
		len += strlen(head->name) + 1;
		head = head->next;
	}
	len += strlen(head->name) + 1;
	char *name = (char *) malloc(len);
	if (!name) return NULL;
	head = s;
	while (head->next != NULL) {
		if (strlen(head->name) != 0) {
			snprintf(name + off, strlen(head->name) + 2, "%s.", head->name);
			off += strlen(head->name) + 1;
		}
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
	if (!name_s)
		return NULL;
	char *full_name =
		(char *) malloc(strlen(name) + strlen(name_s) + added_len + 2);
	if (!full_name) {
		free(name_s);
		return NULL;
	}
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

