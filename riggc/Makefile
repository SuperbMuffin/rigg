CC         = clang
RIGG_ROOT  = $(abspath ..)
CFLAGS     = -Wall -Wextra -Werror -pedantic -std=c99 -g -Iinclude -MMD -MP \
             -DRIGG_RUNTIME_PATH=\"$(RIGG_ROOT)/runtime/cast.c\"

SRC     = src/lexer.c src/main.c src/parser.c src/sema.c src/project.c src/util.c src/codegen.c src/diag.c
OBJ     = $(SRC:src/%.c=build/%.o)
DEP     = $(OBJ:.o=.d)
TARGET  = build/riggc

all: $(TARGET)

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
	cp $(TARGET) /usr/local/bin/riggc

.PHONY: all clean run
