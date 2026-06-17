#!/bin/bash

set -e

TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$TESTS_DIR")"
SRC_DIR="$ROOT_DIR/src"
INCLUDE_DIR="$ROOT_DIR/include"
COMMON_DIR="$TESTS_DIR/common"
CC="${CC:-clang}"

GREEN="\033[0;32m"
RED="\033[0;31m"
CYAN="\033[0;36m"
BOLD="\033[1m"
RESET="\033[0m"

passed=0
failed=0

print_with_streaks() {
  streak=0
  first_name=""

  flush_streak() {
    if [ "$streak" -eq 1 ]; then
      echo -e "PASS  [${first_name}]"
    elif [ "$streak" -gt 1 ]; then
      echo -e "PASS  ${GREEN}${BOLD}x${streak}${RESET}"
    fi
    streak=0
    first_name=""
  }

  while IFS= read -r line; do
    if [[ "$line" == PASS* ]]; then
      name="${line#PASS  [}"
      name="${name%]}"
      if [ "$streak" -eq 0 ]; then
        first_name="$name"
      fi
      streak=$((streak + 1))
    else
      flush_streak
      echo -e "$line"
    fi
  done

  flush_streak
}

for test_file in "$TESTS_DIR"/*/test_*.c; do
  suite_dir="$(dirname "$test_file")"
  suite_name="$(basename "$suite_dir")"

  [ "$suite_name" = "common" ] && continue

  binary="$suite_dir/test_$suite_name"

  echo -e "${BOLD}${CYAN}=============================${RESET}"
  echo -e "${BOLD}${CYAN}==> $suite_name${RESET}"

  extra_sources=""
  if [ -f "$suite_dir/deps" ]; then
    while IFS= read -r dep; do
      if [ -f "$SRC_DIR/$dep" ]; then
        extra_sources="$extra_sources $SRC_DIR/$dep"
      elif [ -f "$TESTS_DIR/$dep" ]; then
        extra_sources="$extra_sources $TESTS_DIR/$dep"
      fi
    done < "$suite_dir/deps"
  fi

  # If the suite has a fixtures/ subdirectory, inject its absolute path so
  # tests can reference fixtures without hardcoding paths.
  extra_defines=""
  if [ -d "$suite_dir/fixtures" ]; then
    extra_defines="-DFIXTURES_DIR=\"$suite_dir/fixtures\""
  fi

  if "$CC" -Wall -Wextra \
      -I"$INCLUDE_DIR" \
      -I"$COMMON_DIR" \
      $extra_defines \
      "$test_file" \
      "$SRC_DIR/$suite_name.c" \
      "$COMMON_DIR/test_common.c" \
      $extra_sources \
      -o "$binary" 2>&1; then
    "$binary" | print_with_streaks
    if [ "${PIPESTATUS[0]}" -eq 0 ]; then
      echo -e "${GREEN}PASSED${RESET}"
      passed=$((passed + 1))
    else
      echo -e "${RED}FAILED${RESET}"
      failed=$((failed + 1))
    fi
  else
    echo -e "${RED}COMPILE FAILED${RESET}"
    failed=$((failed + 1))
  fi

  rm -f "$binary"
  echo ""
done


# ── e2e tests ────────────────────────────────────────────────────────────────
if [ -f "$TESTS_DIR/e2e/run.sh" ]; then
  echo -e "${BOLD}${CYAN}=============================${RESET}"
  echo -e "${BOLD}${CYAN}==> e2e${RESET}"
  if bash "$TESTS_DIR/e2e/run.sh"; then
    echo -e "${GREEN}PASSED${RESET}"
    passed=$((passed + 1))
  else
    echo -e "${RED}FAILED${RESET}"
    failed=$((failed + 1))
  fi
  echo ""
fi

echo -e "${BOLD}-----------------------------${RESET}"
echo -e "suites passed: ${GREEN}${BOLD}$passed${RESET}"
echo -e "suites failed: ${RED}${BOLD}$failed${RESET}"

[ "$failed" -eq 0 ]
