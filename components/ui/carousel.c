#include "carousel.h"

void carousel_init(carousel_t *c, int count) {
    c->count = count; c->current = 0;
    for (int i = 0; i < 8; i++) c->enabled[i] = true;
}
void carousel_set_enabled(carousel_t *c, int idx, bool on) { c->enabled[idx] = on; }
int carousel_current(const carousel_t *c) { return c->current; }
int carousel_next(carousel_t *c) {
    for (int step = 1; step <= c->count; step++) {
        int idx = (c->current + step) % c->count;
        if (c->enabled[idx]) { c->current = idx; return idx; }
    }
    return c->current;  // все выключены — стоим
}
