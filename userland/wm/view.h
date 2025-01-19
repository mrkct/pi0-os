#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <api/syscalls.h>
#include <libgfx/libgfx.h>
#include "tmt.h"


typedef struct View {
    struct Display *display;
    bool dirty;
    TMT *vt;
    int selected_window_idx;
    struct DateTime last_time;

    struct {
        bool blink_state;
        int row, col;
    } cursor;
} View;

void view_init(struct View*, struct Display *display);

void view_update_clock(struct View*, struct DateTime*);

void view_terminal_write(struct View*, char *data, size_t size);

void view_idle_tick(struct View*);

void view_refresh_display(struct View*);

#endif
