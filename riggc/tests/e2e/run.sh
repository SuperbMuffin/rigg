#!/usr/bin/env bash
set -e

E2E_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$(dirname "$E2E_DIR")")"
RIGGC="$ROOT_DIR/build/riggc"

if ! make -C "$ROOT_DIR" -s > /dev/null 2>&1; then
  echo -e "FAIL  [e2e] build failed"
  exit 1
fi
FIXTURES_DIR="$E2E_DIR/fixtures"

GREEN="\033[0;32m"
RED="\033[0;31m"
RESET="\033[0m"
BOLD="\033[1m"

passed=0
failed=0

run_e2e() {
  local name="$1"
  local fixture="$FIXTURES_DIR/$name"
  local expect_exit="${2:-0}"
  local build_dir="$fixture/build"
  local actual_exit=0

  rm -rf "$build_dir"

  if ! "$RIGGC" --emit-ir "$fixture" > /dev/null 2>&1; then
    echo -e "FAIL  [$name] riggc failed to emit IR"
    failed=$((failed + 1))
    return
  fi

  local out="$build_dir/out"
  local runtime="$(dirname "$ROOT_DIR")/runtime/cast.c"
  if ! clang "$build_dir"/ir/*.ll "$runtime" -o "$out" 2>/dev/null; then
    echo -e "FAIL  [$name] clang failed to link"
    failed=$((failed + 1))
    return
  fi

  "$out" || actual_exit=$?
  actual_exit="${actual_exit:-0}"
  rm -rf "$build_dir"

  if [ "$actual_exit" -eq "$expect_exit" ]; then
    echo -e "PASS  [$name]"
    passed=$((passed + 1))
  else
    echo -e "FAIL  [$name] expected exit $expect_exit, got $actual_exit"
    failed=$((failed + 1))
  fi
}

run_e2e "ok_hello"          0
run_e2e "ok_return_code"    42
run_e2e "ok_multi_concept"  42
run_e2e "ok_str_eq"          0

echo ""
echo -e "e2e: ${GREEN}${BOLD}$passed passed${RESET}, ${RED}${BOLD}$failed failed${RESET}"
[ "$failed" -eq 0 ]
