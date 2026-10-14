#!/usr/bin/env bash
# Сборка инструментирующего прохода в разделяемую библиотеку (new Pass Manager).
set -euo pipefail
cd "$(dirname "$0")"

LLVM_CONFIG="${LLVM_CONFIG:-llvm-config}"
INC="$("$LLVM_CONFIG" --includedir)"
CXX="${CXX:-clang++}"

EXTRA=""
# macOS: символы LLVM подтягиваются из процесса opt/clang во время загрузки.
if [[ "$(uname -s)" == "Darwin" ]]; then EXTRA="-undefined dynamic_lookup"; fi

mkdir -p build
# shellcheck disable=SC2086
"$CXX" -std=c++17 -fPIC -shared -I"$INC" $EXTRA \
    TraceUsePass.cpp -o build/libTraceUsePass.so

echo "built: build/libTraceUsePass.so  (LLVM $("$LLVM_CONFIG" --version))"
