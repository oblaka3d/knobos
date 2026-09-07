#ifndef CAROUSEL_H
#define CAROUSEL_H

#include <stdbool.h>

typedef struct { int count; int current; bool enabled[8]; } carousel_t;

void carousel_init(carousel_t *c, int count);           // все экраны enabled
void carousel_set_enabled(carousel_t *c, int idx, bool on);
int  carousel_next(carousel_t *c);                      // след. enabled-экран, «по кругу»; возвращает новый current
int  carousel_current(const carousel_t *c);

#endif
