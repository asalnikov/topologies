#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "jsmn.h"

void
error (const char * errmsg, ...)
{
	va_list args;
	va_start(args, errmsg);
	fprintf(stderr, errmsg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

off_t
read_file (int argc, char *argv[], void **addr)
{
	if (argc != 2)
		error("usage: %s config.json\n", argv[0]);

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		error("could not open %s: %s\n", argv[1], strerror(errno));

	struct stat st;
	if (fstat(fd, &st) < 0)
		error("could not stat %s: %s\n", argv[1], strerror(errno));

	*addr = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*addr == MAP_FAILED)
		error("could not mmap %s: %s\n", argv[1], strerror(errno));

	close(fd);
	return st.st_size;
}

int
main (int argc, char *argv[])
{
	void *addr = NULL;

	off_t file_size = read_file(argc, argv, &addr);

	jsmntok_t *tokens;
	jsmn_parser parser;
	jsmn_init(&parser);
	long long  n_tokens = jsmn_parse(&parser, addr, file_size, NULL, 0);
	if (n_tokens < 0)
		error("%s: invalid JSON data: %d\n", argv[1], n_tokens);
		
	tokens = malloc(sizeof(jsmntok_t) * n_tokens);
	jsmn_parse(&parser, addr, file_size, tokens, n_tokens);


	exit(EXIT_SUCCESS);
}
