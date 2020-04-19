CC = gcc
CFLAGS = -Wall -Wextra -Werror -Og -g -std=gnu99 \
	-Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition # -pedantic -Wconversion
CFLAGS_TINYEXPR = -ansi -Wall -Wshadow -O2
SRC_DIR = src

OBJFILES = $(patsubst %, $(SRC_DIR)/%, name_stack.o param_stack.o graph.o parser.o topologies.o errors.o)

main: $(SRC_DIR)/main.o libtopologies.so
	$(CC) -L. -Wl,-rpath=$(CURDIR) $< -o $@ -ltopologies -lm

libtopologies.so: $(OBJFILES) $(SRC_DIR)/tinyexpr.o
	$(CC) -shared -fPIC -Wl,--version-script=visibility.map $^ -o $@ -lm

$(SRC_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJFILES): %.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(SRC_DIR)/tinyexpr.o: $(SRC_DIR)/tinyexpr.c
	$(CC) $(CFLAGS_TINYEXPR) -c $^ -o $@

clean:
	rm -f main $(SRC_DIR)/*.o *.so

.PHONY: clean
