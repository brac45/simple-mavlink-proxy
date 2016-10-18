#
# Makefile for managing the build of the project
#

# ========== Variables ==============
# Main compiler
CC := gcc
#		-Wall : give verbose compiler warnings
#		-g : compile with debugging info
CFLAGS := -g -Wall

#	Directory and main executable variable
SRCDIR := src
BUILDDIR := build
TARGET := bin/main_exe

# File extensions and some more options
SRCEXT := c
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
LIBRARY := -I lib/common/
INCLUDE := -I include

# =========== Build procedure ========
$(TARGET): $(OBJECTS)
	@echo "Linking.."
	$(CC) $^ -o $(TARGET)

$(OBJECTS): $(SOURCES) directory
	@echo "Creating Objects.."
	$(CC) $(CFLAGS) $(INCLUDE) $(LIBRARY) -c -o $@ $<

directory:
	mkdir -p build
	mkdir -p bin
	mkdir -p logs

clean:
	@echo "Cleaning.."
	rm -rv bin/*
	rm -rv build/*

.PHONY: clean
