# Variables
SRCDIR := .
BUILDDIR := build
TARGET := fat32

# Source files
SOURCES := $(wildcard $(SRCDIR)/*.c)
HEADERS := $(wildcard $(SRCDIR)/*.h)

# Object files
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

# Compiler flags
CC := gcc
CFLAGS := -Wall -Wextra
LDFLAGS :=

# Debug mode
ifeq ($(DEBUG),1)
	CFLAGS += -g -O0
	LDFLAGS += -g
else
	CFLAGS += -O2
endif

# Build targets
all: $(BUILDDIR)/$(TARGET)

$(BUILDDIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

cleanall:
	@rm -rf $(BUILDDIR)

cleanobj:
	@rm -f $(OBJECTS)

.PHONY: all clean cleanobj
