#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$(mktemp -d /tmp/zo-task-XXXXXX)"
trap 'rm -rf "$build_dir"' EXIT
compiler="${CXX:-g++}"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -I "$repo_dir/Plugins/DarkRelicCore/Source/DarkRelicCore/Public")
"$compiler" "${flags[@]}" "$repo_dir/tests/core_test.cpp" -o "$build_dir/core-tests"
"$build_dir/core-tests"
"$compiler" "${flags[@]}" "$repo_dir/examples/demo.cpp" -o "$build_dir/demo"
"$build_dir/demo"
