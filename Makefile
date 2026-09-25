PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=
INSTALL ?= install

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror
CJSON_CFLAGS = $(shell pkg-config --cflags libcjson)
CJSON_LIBS = $(shell pkg-config --libs libcjson)
TZDIR ?= /usr/share/zoneinfo
DATE_CFLAGS := -DDATE_ZONEINFO_DIR=\"$(TZDIR)\"
CORE := src/util.c src/tools.c src/date.c
HEADERS := src/common.h src/tools.h src/date.h

.PHONY: all test install uninstall clean

all: himalaya-mcp

himalaya-mcp: src/main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) $(DATE_CFLAGS) -o $@ src/main.c $(CORE) $(CJSON_LIBS)

protocol_test: tests/protocol_test.c
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) -o $@ tests/protocol_test.c $(CJSON_LIBS)

mail_test: tests/mail_test.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) $(DATE_CFLAGS) -Isrc -o $@ tests/mail_test.c $(CORE) $(CJSON_LIBS)

capabilities_test: tests/capabilities_test.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) $(CJSON_CFLAGS) $(DATE_CFLAGS) -Isrc -o $@ tests/capabilities_test.c $(CORE) $(CJSON_LIBS)

date_test: tests/date_test.c src/date.c src/date.h
	$(CC) $(CFLAGS) $(DATE_CFLAGS) -Isrc -o $@ tests/date_test.c src/date.c

test: himalaya-mcp protocol_test mail_test capabilities_test date_test
	./protocol_test ./himalaya-mcp
	./mail_test
	./capabilities_test
	./date_test

install: himalaya-mcp
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 755 himalaya-mcp "$(DESTDIR)$(BINDIR)/himalaya-mcp"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/himalaya-mcp"

clean:
	rm -f himalaya-mcp protocol_test mail_test capabilities_test date_test
