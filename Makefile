# pfind & spfind — build with gcc or clang, C99, POSIX
CC ?= gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=gnu99
SRCDIR = src

PFIND = pfind
SPFIND = spfind

.PHONY: all clean test

all: $(PFIND) $(SPFIND)

$(PFIND): $(SRCDIR)/pfind.c
	$(CC) $(CFLAGS) -o $(PFIND) $(SRCDIR)/pfind.c

$(SPFIND): $(SRCDIR)/spfind.c
	$(CC) $(CFLAGS) -o $(SPFIND) $(SRCDIR)/spfind.c

clean:
	rm -f $(PFIND) $(SPFIND)

test:
	@./scripts/run_tests.sh
