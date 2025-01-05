#include <stdio.h>
#include <libgfx/libgfx.h>
#include "view.h"


static void redraw_top_bar(struct View *view)
{
    char message[128];
    sprintf(message, "% 2d:%02d - % 4d-%02d-%02d",
        view->last_time.hour, view->last_time.minute,
        view->last_time.year, view->last_time.month,
        view->last_time.day
    );

    draw_filled_rect(view->display, 0, 0, view->display->width, 20, COL_BLACK);
    draw_text(view->display, get_default_font(), message, 10, 10, COL_WHITE);
}

void view_init(struct View *view, struct Display *display)
{
    *view = (struct View) {
        .display = display,
        .dirty = true,
        .last_time = {
            .year = 1998,
            .month = 7,
            .day = 25,
            .hour = 12,
            .minute = 0,
            .second = 0,
        },
    };
}

void view_update_clock(struct View *view, struct DateTime *dt)
{
    if (datetime_compare(&view->last_time, dt) == 0)
        return;

    view->last_time = *dt;
    view->dirty = true;
    redraw_top_bar(view);
}

void view_terminal_write(struct View *view, char *data, size_t size)
{
    view->dirty = true;
    // TODO
}

void view_refresh_display(struct View *view)
{
    if (!view->dirty)
        return;
    view->dirty = false;
    view->display->ops.refresh(view->display);
}
