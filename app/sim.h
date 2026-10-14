#ifndef SIM_H
#define SIM_H

/* Интерфейс графической "песочницы" курса llvm_course.
 * Логический модуль app.c использует только эти функции; их реализует либо
 * настоящий SDL2-sim (sim.c из курса), либо headless-заглушка sim_stub.c. */

#define SIM_X_SIZE 1536
#define SIM_Y_SIZE 768

void simInit(void);
void app(void);
void simExit(void);
void simFlush(void);
void simPutPixel(int x, int y, int argb);
int simRand(void);
int simHasClick(void);
int simGetClick(void);

#endif /* SIM_H */
