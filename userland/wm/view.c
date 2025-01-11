#include <stdio.h>
#include <libgfx/libgfx.h>
#include "view.h"


#define TOPBAR_HEIGHT   (1*CHAR_HEIGHT)
#define HPADDING        16
#define VPADDING        10
#define FONT_HMARGIN    2
#define FONT_VMARGIN    4
#define SCALE           2
#define CHAR_WIDTH      (SCALE * (8 + FONT_HMARGIN))
#define CHAR_HEIGHT     (SCALE * (9 + FONT_VMARGIN))

#define CHARX(x)        (HPADDING + (x) * CHAR_WIDTH)
#define CHARY(y)        (TOPBAR_HEIGHT + VPADDING + (y) * CHAR_HEIGHT)


static void redraw_top_bar(struct View *view)
{
    char message[128];
    int len = sprintf(message, "% 2d:%02d %d-%02d-%02d",
        view->last_time.hour, view->last_time.minute,
        view->last_time.year, view->last_time.month,
        view->last_time.day
    );
    int text_width = CHAR_WIDTH * len;

    draw_filled_rect(view->display, 0, 0, view->display->width, TOPBAR_HEIGHT, COL_GRAY);
    draw_text(view->display, get_default_font(), message, view->display->width - CHAR_WIDTH - text_width, 7, SCALE, COL_BLACK);

    for (int i = 0; i < 10; i++) {
        uint32_t color = (i == view->selected_window_idx) ? COL_WHITE : COL_BLACK;
        draw_char(view->display, get_default_font(), '0' + i, CHARX(i), 7, SCALE, color);
    }
}

static uint32_t tmtcolor_to_ours(tmt_color_t col, uint32_t default_color)
{
    switch (col) {
        case TMT_COLOR_BLACK:   return COL_BLACK;
        case TMT_COLOR_RED:     return COL_RED;
        case TMT_COLOR_GREEN:   return COL_GREEN;
        case TMT_COLOR_YELLOW:  return COL_YELLOW;
        case TMT_COLOR_BLUE:    return COL_BLUE;
        case TMT_COLOR_MAGENTA: return COL_MAGENTA;
        case TMT_COLOR_CYAN:    return COL_CYAN;
        case TMT_COLOR_WHITE:   return COL_WHITE;
        case TMT_COLOR_DEFAULT:
        default:
            return default_color;
    }
}

static void draw_term_char(struct View *view, size_t x, size_t y, char c, TMTATTRS *attrs)
{
    Font *font = get_default_font();
    int scale = SCALE;
    if (attrs->bold)
        scale += 1;

    draw_filled_rect(view->display, CHARX(x), CHARY(y), CHAR_WIDTH + font->hmargin, CHAR_HEIGHT + font->vmargin, tmtcolor_to_ours(attrs->bg, COL_BLACK));
    draw_char(view->display, font, c, CHARX(x), CHARY(y), scale, tmtcolor_to_ours(attrs->fg, COL_GRAY));
    if (attrs->underline)
        draw_line(view->display, CHARX(x), CHARY(y) + CHAR_HEIGHT, CHARX(x) + CHAR_WIDTH, CHARY(y) + CHAR_HEIGHT, 3, tmtcolor_to_ours(attrs->fg, COL_GRAY));
}

static void tmtcb(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    /* grab a pointer to the virtual screen */
    const TMTSCREEN *s = tmt_screen(vt);
    const TMTPOINT *c = tmt_cursor(vt);
    struct View *view = (struct View *) p;

    switch (m){
        case TMT_MSG_BELL:
            /* the terminal is requesting that we ring the bell/flash the
             * screen/do whatever ^G is supposed to do; a is NULL
             */
            printf("bing!\n");
            break;

        case TMT_MSG_UPDATE:
            printf("screen update\n");
            /* the screen image changed; a is a pointer to the TMTSCREEN */
            for (size_t r = 0; r < s->nline; r++){
                if (s->lines[r]->dirty){
                    for (size_t c = 0; c < s->ncol; c++){
                        TMTCHAR *ch = &s->lines[r]->chars[c];
                        draw_term_char(view, c, r, ch->c, &ch->a);
                    }
                }
            }

            /* let tmt know we've redrawn the screen */
            tmt_clean(vt);
            view->dirty = true;
            break;

        case TMT_MSG_ANSWER:
            /* the terminal has a response to give to the program; a is a
             * pointer to a string */
            printf("terminal answered %s\n", (const char *)a);
            break;

        case TMT_MSG_MOVED:
            /* the cursor moved; a is a pointer to the cursor's TMTPOINT */
            printf("cursor is now at %zd,%zd\n", c->r, c->c);
            break;
    }
}

void view_init(struct View *view, struct Display *display)
{
    size_t nlines = (display->height - 2*VPADDING) / CHAR_HEIGHT;
    size_t ncols = (display->width - 2*HPADDING) / CHAR_WIDTH;

    /**
     * tmt_open() ends up calling the callback immediately
     * to ask us to refresh the screen, so the view ptr
     * we need to pass it must already have enough data
     * for us to do that.
     * 
     * so we first initialize everything except .vt in *view,
     * call tmt_open and fill in the rest of the struct.
     */

    *view = (struct View) {
        .display = display,
        .dirty = true,
        .selected_window_idx = 0,
        .last_time = {
            .year = 1998,
            .month = 7,
            .day = 25,
            .hour = 12,
            .minute = 0,
            .second = 0,
        },
    };

    TMT *vt = tmt_open(nlines, ncols, tmtcb, view, NULL);
    if (!vt) {
        fprintf(stderr, "Failed to initialize terminal\n");
        sys_exit(-1);
    }
    view->vt = vt;
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
    tmt_write(view->vt, data, size);
}

void view_refresh_display(struct View *view)
{
    if (!view->dirty)
        return;
    view->dirty = false;
    view->display->ops.refresh(view->display);
}
