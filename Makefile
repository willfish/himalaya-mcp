CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror
CJSON_CFLAGS := $(shell pkg-config --cflags libcjson)
CJSON_LIBS := $(shell pkg-config --libs libcjson)

.PHONY: all test clean

all: himalaya-mcp

himalaya-mcp: src/main.c src/util.c src/tools.c src/common.h src/tools.h
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) -o $@ src/main.c src/util.c src/tools.c $(CJSON_LIBS)

protocol_test: tests/protocol_test.c
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) -o $@ tests/protocol_test.c $(CJSON_LIBS)

test: himalaya-mcp protocol_test
	./protocol_test ./himalaya-mcp

clean:
	rm -f himalaya-mcp protocol_test
