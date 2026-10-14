/* start.c — точка входа. Компилируется БЕЗ инструментирующего прохода. */
#include "sim.h"

int main(void) {
  simInit();
  app();
  simExit();
  return 0;
}
