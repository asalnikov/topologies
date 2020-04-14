CC = gcc
CFLAGS = -Wall -Wextra -Werror -Og -g -std=gnu99 \
	-Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition # -pedantic -Wconversion
CFLAGS_TINYEXPR = -ansi -Wall -Wshadow -O2

OBJFILES = name_stack.o param_stack.o graph.o parser.o topologies.o

main: main.o libtopologies.so
	$(CC) -L. -Wl,-rpath=$(CURDIR) $< -o $@ -ltopologies -lm

libtopologies.so: $(OBJFILES) tinyexpr.o
	$(CC) -shared -fPIC -Wl,--version-script=visibility.map $^ -o $@ -lm

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJFILES): %.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

tinyexpr.o: tinyexpr.c
	$(CC) $(CFLAGS_TINYEXPR) -c $^ -o $@

clean:
	rm -f main *.o

.PHONY: clean
