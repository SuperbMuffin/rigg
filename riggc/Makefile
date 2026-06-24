CC            = clang
RIGG_ROOT     = $(abspath ..)
CFLAGS_COMMON = -Wall -Wextra -Werror -pedantic -std=c99 -g -Iinclude -MMD -MP \
             -DRIGG_RUNTIME_PATH=\"$(RIGG_ROOT)/runtime/cast.c\"
CFLAGS_DEV    = -Og
CFLAGS_PROD   = -O2

ifeq ($(BUILD_MODE),prod)
    CFLAGS_BUILD = $(CFLAGS_PROD)
else
    CFLAGS_BUILD = $(CFLAGS_DEV)
endif

SRC     = src/lexer.c src/main.c src/parser.c src/sema.c src/project.c src/util.c src/codegen.c src/diag.c
OBJ     = $(SRC:src/%.c=build/%.o)
DEP     = $(OBJ:.o=.d)
TARGET  = build/riggc
BUILD_MODE ?= dev

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p build
	@echo "[LD] $@"
	@$(CC) $(CFLAGS_COMMMON) $(CFLAGS_BUILD) $(OBJ) -o $(TARGET)

build/%.o: src/%.c
	@mkdir -p build
	@echo "[CC] $<"
	@$(CC) $(CFLAGS_COMMON) $(CFLAGS_BUILD) -c $< -o $@

-include $(DEP)

clean:
	rm -rf build

test:
	bash tests/run_tests.sh

run: all
	./$(TARGET)

install: all
	cp $(TARGET) /usr/local/bin/riggc

prod:
	@$(MAKE) --no-print-directory BUILD_MODE=prod all
	@echo "--- Prod Build Built ---"
.PHONY: all clean run
