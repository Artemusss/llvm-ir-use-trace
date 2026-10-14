/* sim_stub.c — headless-реализация sim-интерфейса БЕЗ окна/SDL2.
 *
 * Нужна только чтобы собрать трассу: инструментирование живёт в app.c и работает
 * одинаково, рисуем мы пиксели на экран или нет. Заглушка позволяет прогнать
 * app() на машине без дисплея/SDL2 (CI, сервер) и получить ту же статистику.
 *
 * SIM_FRAMES (env) — сколько кадров выполнить, затем аккуратный exit(0),
 * чтобы отработал деструктор логгера и трасса закрылась. По умолчанию 3.
 * Для настоящего графического окна используйте sim.c из курса + -lSDL2.
 */
#include "sim.h"
#include <stdlib.h>

void simInit(void) {}
void simExit(void) {}

void simFlush(void) {
  static int frames = 0;
  static int limit = -1;
  if (limit < 0) {
    const char *e = getenv("SIM_FRAMES");
    limit = (e && *e) ? atoi(e) : 3;
  }
  if (++frames >= limit)
    exit(0); /* exit() вызывает деструкторы -> трасса флашится и закрывается */
}

void simPutPixel(int x, int y, int argb) {
  (void)x;
  (void)y;
  (void)argb;
}

int simRand(void) { return rand(); }
int simHasClick(void) { return 0; }
int simGetClick(void) { return 0; }
