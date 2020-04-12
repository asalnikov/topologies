CC = gcc
CFLAGS = -Wall -Wextra -Werror -Og -g -std=gnu99 \
	-Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition # -pedantic -Wconversion
CFLAGS_TINYEXPR = -ansi -Wall -Wshadow -O2

main: graph.o tinyexpr.o parser.o main.o
	$(CC) $(CFLAGS) $^ -o $@ -lm

main.o: main.c
	$(CC) $(CFLAGS) -c $^ -o $@

parser.o: parser.c
	$(CC) $(CFLAGS) -c $^ -o $@

graph.o: graph.c
	$(CC) $(CFLAGS) -c $^ -o $@

tinyexpr.o: tinyexpr.c
	$(CC) $(CFLAGS_TINYEXPR) -c $^ -o $@

clean:
	rm -f main *.o

.PHONY: clean
