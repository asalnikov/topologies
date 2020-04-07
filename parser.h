#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "jsmn.h"
#include "defs.h"

void
json_deserialize (jsmntok_t *tokens, int n_tokens, char *text,
                  network_definition_t *network_definition);
void
json_print (jsmntok_t *tokens, int n_tokens, char *text);
