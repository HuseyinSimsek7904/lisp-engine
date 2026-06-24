# Project directory layout
SRCDIR		?= ./src

# Output directories
OBJDIR		?= ./obj
BINDIR		?= ./bin
EXECUTABLE	?= $(BINDIR)/lisp

SOURCES		:= $(shell find $(SRCDIR) -name '*.c')
OBJECTS		:= $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJDIRS		:= $(sort $(dir $(OBJECTS)))
DEPENDS		:= $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.d, $(SOURCES))

CC		:= gcc
CFLAGS		:= -Wall -Werror
CPPFLAGS	:= -I src/ $(CMACROS)

EXTDEPS		:= readline
EXTCFLAGS	:= $(shell pkg-config --cflags --libs $(EXTDEPS))

.PHONY: all debug build \
	clean gen-bear tests

all: debug

debug: CFLAGS += -ggdb
debug: $(OBJDIRS) $(EXECUTABLE)

build: CFLAGS += -O3
build: CPPFLAGS += -DNDEBUG
build: $(OBJDIRS) $(EXECUTABLE)

# Header dependencies
-include $(DEPENDS)

# Building
$(BINDIR):
	mkdir -p $(BINDIR)

$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXTCFLAGS) $^ -o $@

$(OBJDIRS):
	mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS)  $(EXTCFLAGS) -MMD -MP -c $< -o $@

# Miscellaneous
clean:
	rm -rf $(OBJDIR)
	rm -rf $(BINDIR)

gen-bear: clean
	bear -- make debug
