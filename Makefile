CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS += -Iinclude -Isrc
LDFLAGS +=
LDLIBS += -lm

LIB_SRC = src/jalalix.c src/tz.c src/tzdata_embed.c
LIB_OBJ = src/jalalix.o src/tz.o src/tzdata_embed.o

.PHONY: all clean test example

all: $(LIB_OBJ) tests/test_jalalix examples/basic

src/%.o: src/%.c include/jalalix.h src/tz.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

tests/test_jalalix: tests/test_jalalix.c $(LIB_OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_jalalix.c $(LIB_OBJ) $(LDFLAGS) $(LDLIBS)

examples/basic: examples/basic.c $(LIB_OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ examples/basic.c $(LIB_OBJ) $(LDFLAGS) $(LDLIBS)

test: tests/test_jalalix
	./tests/test_jalalix

example: examples/basic
	./examples/basic

clean:
	rm -f $(LIB_OBJ) tests/test_jalalix examples/basic
