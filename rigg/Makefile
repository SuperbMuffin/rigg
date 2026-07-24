CC      = clang
PREFIX ?= /usr/local
DESTDIR ?=
CORE_PATH ?= /usr/local/share/rigg/core
CFLAGS  = -Wall -Wextra -Werror -pedantic -std=c99 -g -Iinclude -MMD -MP -DRIGG_CORE_PATH=\"$(CORE_PATH)\"

SRC     = src/main.c src/cmd.c src/toml.c
OBJ     = $(SRC:src/%.c=build/%.o)
DEP     = $(OBJ:.o=.d)
TARGET  = build/rigg

all: $(TARGET)

build: all

$(TARGET): $(OBJ)
	@mkdir -p build
	@echo "[LD] $@"
	@$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

build/%.o: src/%.c
	@mkdir -p build
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

clean:
	rm -rf build

test:
	bash tests/run_tests.sh

run: all
	./$(TARGET)

install: all
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(TARGET) $(DESTDIR)$(PREFIX)/bin/rigg

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/rigg

.PHONY: all build clean run install uninstall test
