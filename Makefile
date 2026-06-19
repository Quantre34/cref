CC      = gcc
TARGET  = cref
SRCS    = main.c meta.c search.c tui.c term.c notes.c
OBJS    = $(SRCS:.c=.o)

PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/cref/ref

UNAME   := $(shell uname -s)

ifeq ($(UNAME), Darwin)
    CFLAGS   = -Wall -Wextra -std=c11 -g -I/opt/homebrew/include
    LDFLAGS  = -lncurses -lpthread -lsodium \
               -framework IOKit -framework CoreFoundation \
               -L/opt/homebrew/lib
else
    CFLAGS   = -Wall -Wextra -std=c11 -g
    LDFLAGS  = -lncurses -lpthread -lsodium -lutil
endif

# ---------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(UNAME), Darwin)
release: CFLAGS = -Wall -Wextra -std=c11 -O2 -DNDEBUG -I/opt/homebrew/include
else
release: CFLAGS = -Wall -Wextra -std=c11 -O2 -DNDEBUG
endif
release: $(TARGET)

# Dependencies
main.o:   main.c meta.h tui.h notes.h
meta.o:   meta.c meta.h
search.o: search.c search.h meta.h
tui.o:    tui.c tui.h meta.h search.h term.h notes.h
term.o:   term.c term.h
notes.o:  notes.c notes.h

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
