#!/usr/bin/env bash
# Полный конвейер: оптимизированный IR app.c -> инструментирование -> запуск -> анализ.
#
#   OPT=-O2|-Os   уровень оптимизации (по умолчанию -O2)
#   SIM=app/sim_stub.c   реализация sim; app/sim.c (из курса) для настоящего окна
#   TRACE_MAX, SIM_FRAMES — см. runtime/trace_rt.c и app/sim_stub.c
#
# Инструментируется ТОЛЬКО app.c; sim/start компилируются без прохода.
set -euo pipefail
cd "$(dirname "$0")"

OPT="${OPT:--O2}"
SIM="${SIM:-app/sim_stub.c}"
LVL="${OPT#-}"
PLUGIN="build/libTraceUsePass.so"

[[ -f "$PLUGIN" ]] || { echo "сначала ./build.sh"; exit 1; }
mkdir -p build stats

echo "==> 1/4 оптимизированный IR только app.c ($OPT)"
clang "$OPT" -emit-llvm -S app/app.c -o "build/app.$LVL.ll"

echo "==> 2/4 инструментирование прохода trace-use"
opt -load-pass-plugin "$PLUGIN" -passes=trace-use \
    "build/app.$LVL.ll" -S -o "build/app.$LVL.traced.ll"

echo "==> 3/4 линковка (app.traced + sim + start + рантайм) и запуск"
SDL_FLAGS=""
if [[ "$(basename "$SIM")" == "sim.c" ]]; then SDL_FLAGS="$(pkg-config --cflags --libs sdl2)"; fi
# shellcheck disable=SC2086
clang "$OPT" "build/app.$LVL.traced.ll" "$SIM" app/start.c runtime/trace_rt.c \
    $SDL_FLAGS -o build/app_traced
TRACE_FILE="build/trace.log" ./build/app_traced || true   # sim_stub выходит exit(0)

echo "==> 4/4 анализ паттернов (окна 1..5)"
python3 scripts/analyze.py build/trace.log -o stats -k 20 -n 5
