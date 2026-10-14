# IR Use-Trace Pass

Инструментирующий LLVM-проход (new Pass Manager), который **в рантайме** собирает
трассу использования IR-инструкций `User <- Operand` графического приложения курса
[lisitsynSA/llvm_course](https://github.com/lisitsynSA/llvm_course), инструментируя
**только логический модуль `app.c`** на `-O2`/`-Os`, и скрипт анализа часто
повторяемых паттернов.

---

## Установка зависимостей (Ubuntu)

```bash
sudo apt update
sudo apt install -y clang-18 llvm-18 llvm-18-dev cmake python3 libsdl2-dev
# чтобы clang/opt/llvm-config указывали на 18 (если в системе несколько версий):
sudo update-alternatives --install /usr/bin/clang       clang       /usr/bin/clang-18 100
sudo update-alternatives --install /usr/bin/opt         opt         /usr/bin/opt-18 100
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-18 100
```
`libsdl2-dev` нужен только для настоящего графического окна. Для сбора трассы он не
обязателен — есть headless-заглушка `app/sim_stub.c`.

---

## Шаг 1. Собрать проход

```bash
./build.sh                       # -> build/libTraceUsePass.so
```


## Шаг 2–4. Трасса и анализ одной командой

```bash
./run.sh                         # -O2, headless, трасса + stats/
OPT=-Os ./run.sh                 # то же на -Os
```
### Пошагово вручную

1. **Оптимизированный IR только `app.c`** (`-O2` или `-Os`):
   ```bash
   clang -O2 -emit-llvm -S app/app.c -o build/app.O2.ll
   ```
2. **Инструментирование** проходом `trace-use` (через `opt`):
   ```bash
   opt -load-pass-plugin build/libTraceUsePass.so -passes=trace-use \
       build/app.O2.ll -S -o build/app.O2.traced.ll
   ```
   Проход печатает сводку, напр.: `instrumented 13 instrs, 17 use-edges
   (8 from phi expansion)`.
3. **Линковка и запуск.** `sim`, `start` и рантайм-логгер компилируются **без**
   прохода.
   - headless (без дисплея/SDL2, для сервера/CI):
     ```bash
     clang -O2 build/app.O2.traced.ll app/sim_stub.c app/start.c runtime/trace_rt.c \
           -o build/app_traced
     TRACE_FILE=build/trace.log ./build/app_traced
     ```
   - настоящее окно (нужен `libsdl2-dev` и `sim.c` из курса):
     ```bash
     clang -O2 build/app.O2.traced.ll /path/to/llvm_course/SDL/sim.c app/start.c \
           runtime/trace_rt.c $(pkg-config --cflags --libs sdl2) -o build/app_traced
     TRACE_FILE=build/trace.log ./build/app_traced     # закрыть окно для выхода
     ```
4. **Анализ паттернов** (окна 1–5):
   ```bash
   python3 scripts/analyze.py build/trace.log -o stats -k 20 -n 5
   ```
   Пишет `stats/patterns.txt` (топ-20 на окно) и `stats/patterns_n1..n5.csv` (полные).

### Управление объёмом трассы (env)

Горячий цикл графики порождает гигантскую трассу, поэтому логгер ограничивает число
записей — паттерны тела цикла повторяются и так попадают в выборку.

| переменная | по умолч. | смысл |
|---|---|---|
| `TRACE_FILE` | `trace.log` | путь к трассе |
| `TRACE_MAX`  | `2000000`  | максимум записей (`0` — без лимита) |
| `SIM_FRAMES` | `3`        | кадров в headless-заглушке, затем `exit(0)` |

---

## Результаты (эта сборка, `-O2`, 2·10⁶ записей)

Доминируют рёбра и n-граммы горячего внутреннего цикла `simPutPixel(x, y, …x*y*step…)`:

- `call <- add` — 33.3 % всех рёбер (аргументы `simPutPixel`, часть — из разворота phi);
- остальные шесть рёбер (`mul<-add`, `add<-add`, `icmp<-add`, `br<-icmp`, `mul<-mul`,
  `add<-mul`) — по ~11 %.
- Для окон 2–5 топ-паттерны — это подряд идущие срезы тела цикла, каждый ~11 %, напр.
  `mul<-mul , mul<-add , add<-mul , call<-add , call<-add`.

Полные таблицы — в [`stats/patterns.txt`](stats/patterns.txt) и `stats/patterns_n*.csv`;
образец трассы — [`stats/trace_sample.log`](stats/trace_sample.log).

---

## Структура репозитория

```
TraceUsePass.cpp      инструментирующий проход (new PM, пассы opt + fpass-plugin)
runtime/trace_rt.c    рантайм: traceUse() -> строки трассы (ctor/dtor, лимит)
scripts/analyze.py    анализ n-грамм (окна 1..5), patterns.txt + CSV
app/app.c             логический модуль (из курса) — ЕГО инструментируем
app/sim.h start.c     интерфейс и точка входа
app/sim_stub.c        headless-реализация sim (без SDL2) для сбора трассы
build.sh run.sh       сборка прохода и полный конвейер
CMakeLists.txt        альтернативная сборка прохода
stats/                собранная статистика (коммитим это, а не полную трассу)
```
