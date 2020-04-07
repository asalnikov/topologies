CC = gcc
CFLAGS = -Wall -Wextra -Werror -Og -g -std=gnu99

main: main.c parser.c
	$(CC) $(CFLAGS) $< -o $@
