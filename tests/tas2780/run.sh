#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$test_dir/../.." && pwd)
build=$(mktemp -d "$test_dir/.build.XXXXXX")
trap 'rm -rf "$build"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -Wno-unused-const-variable -g -O1 -fno-omit-frame-pointer -fno-sanitize-recover=all \
  ${SANITIZERS--fsanitize=address,undefined} \
  -I "$test_dir/mocks" -I "$root" \
  "$test_dir/test_tas2780.cpp" "$root/esphome/components/tas2780/tas2780.cpp" \
  -o "$build/test_tas2780"
"$build/test_tas2780"
