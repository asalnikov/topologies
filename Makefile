CC = gcc
CFLAGS = -Wall -Wextra -Werror -Og -g -std=gnu99 -pedantic \
	-Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition # -Wconversion

main: parser.o main.o
	$(CC) $(CFLAGS) parser.o main.o -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

parser.o: parser.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f main *.o

.PHONY: clean
