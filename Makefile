CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
TARGET  = cref
SRCS    = main.c meta.c search.c tui.c term.c
OBJS    = $(SRCS:.c=.o)

PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/cref/ref

UNAME   := $(shell uname -s)
LDFLAGS  = -lncurses -lpthread
ifeq ($(UNAME), Linux)
    LDFLAGS += -lutil
endif

# ---------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

release: CFLAGS = -Wall -Wextra -std=c11 -O2 -DNDEBUG
release: $(TARGET)

# Dependencies
main.o:   main.c meta.h tui.h
meta.o:   meta.c meta.h
search.o: search.c search.h meta.h
tui.o:    tui.c tui.h meta.h search.h term.h
term.o:   term.c term.h

# ---------------------------------------------------------------
install: $(TARGET)
	install -d $(BINDIR)
	install -d $(DATADIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	cp -r ref/. $(DATADIR)/
	@echo "Installed: $(BINDIR)/$(TARGET)"
	@echo "Data:      $(DATADIR)"

uninstall:
	rm -f  $(BINDIR)/$(TARGET)
	rm -rf $(PREFIX)/share/cref
	@echo "Uninstalled."

TEST_SEARCH = tests/test_search
TEST_META   = tests/test_meta

$(TEST_SEARCH): tests/test_search.c search.c meta.c search.h meta.h
	$(CC) $(CFLAGS) -o $@ tests/test_search.c search.c meta.c

$(TEST_META): tests/test_meta.c meta.c meta.h
	$(CC) $(CFLAGS) -o $@ tests/test_meta.c meta.c

test: $(TARGET) $(TEST_SEARCH) $(TEST_META)
	@echo "=== unit: search ==="
	@./$(TEST_SEARCH)
	@echo "=== unit: meta ==="
	@./$(TEST_META)
	@echo "=== cli ==="
	@bash tests/test_cli.sh
	@echo ""
	@echo "All tests passed."

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_SEARCH) $(TEST_META)

.PHONY: all clean release install uninstall test
