/* trace_rt.c — рантайм-логгер для инструментирующего прохода trace-use.
 *
 * Проход вставляет вызовы traceUse(user, operand). Здесь они реализованы:
 * каждая запись дописывается строкой "<user> <- <operand>\n" в трассу.
 * Порядок строк = порядок исполнения, поэтому по трассе можно искать
 * часто повторяющиеся последовательности (окна 1..5).
 *
 * Открытие/закрытие файла — через конструктор/деструктор, поэтому
 * инструментировать main() не нужно (main лежит в start.c / sim).
 *
 * Переменные окружения:
 *   TRACE_FILE  — путь к трассе (по умолчанию "trace.log").
 *   TRACE_MAX   — максимум записей; 0 = без ограничения (по умолч. 2000000).
 *                 Горячие циклы графики дают огромную трассу, поэтому ограничение
 *                 нужно: паттерны тела цикла повторяются и так попадают в выборку.
 */
#include <stdio.h>
#include <stdlib.h>

static FILE *g_f = NULL;
static unsigned long long g_count = 0;
static unsigned long long g_cap = 2000000ULL;

__attribute__((constructor)) static void trace_init(void) {
  const char *path = getenv("TRACE_FILE");
  if (!path || !*path)
    path = "trace.log";
  const char *cap = getenv("TRACE_MAX");
  if (cap && *cap)
    g_cap = strtoull(cap, NULL, 10);
  g_f = fopen(path, "w");
  if (g_f)
    setvbuf(g_f, NULL, _IOFBF, 1 << 20); /* 1 MiB буфер — быстрее */
}

__attribute__((destructor)) static void trace_fini(void) {
  if (g_f) {
    fflush(g_f);
    fclose(g_f);
    g_f = NULL;
    fprintf(stderr, "[trace_rt] wrote %llu use-records\n", g_count);
  }
}

/* Символ с C-линковкой: имя ровно "traceUse" — его и вставляет проход. */
void traceUse(const char *user, const char *op) {
  if (!g_f)
    return;
  if (g_cap && g_count >= g_cap)
    return;
  fputs(user, g_f);
  fputs(" <- ", g_f);
  fputs(op, g_f);
  fputc('\n', g_f);
  ++g_count;
}
