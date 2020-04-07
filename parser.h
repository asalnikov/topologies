#ifndef PARSER_H
# define PARSER_H

#include "defs.h"

int
json_read_file (char *text, off_t file_size,
                network_definition_t *network_definition);
#endif
