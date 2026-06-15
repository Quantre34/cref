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

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean release install uninstall
