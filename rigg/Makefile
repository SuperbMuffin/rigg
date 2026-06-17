CC      = clang
CFLAGS  = -Wall -Wextra -Werror -pedantic -std=c99 -g -Iinclude -MMD -MP

SRC     = src/main.c src/cmd.c src/toml.c
OBJ     = $(SRC:src/%.c=build/%.o)
DEP     = $(OBJ:.o=.d)
TARGET  = build/rigg

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
	cp $(TARGET) /usr/local/bin/rigg

.PHONY: all clean run
