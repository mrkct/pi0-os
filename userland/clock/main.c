#include <stdio.h>
#include <stdbool.h>
#include "libmmath.h"
#include "libgfx.h"
#include "libdatetime.h"


void draw_clock(Window *window, DateTime *datetime)
{
    const int CLOCK_RADIUS = window->width / 2 - 24;
    const int HOURS_HAND_LENGTH = 40;
    const int MINUTES_HAND_LENGTH = 72;
    const int SECONDS_HAND_LENGTH = 72;

    const int center_x = window->width / 2;
    const int center_y = window->height / 2;

    draw_filled_rect(window, 0, 0, window->width, window->height, COL_WHITE);
    draw_circle(window, center_x, center_y, CLOCK_RADIUS, COL_BLACK);

    int angle_step = 360 / (12 * 5);
    for (int i = 0; i < 12 * 5; i++) {
        bool is_thick = i % 5 == 0;

        int x1 = window->width / 2 + cos(deg2rad(angle_step * i)) * (CLOCK_RADIUS - 16);
        int y1 = window->width / 2 + sin(deg2rad(angle_step * i)) * (CLOCK_RADIUS - 16);

        const int length = 12;
        int x2 = window->width / 2 + cos(deg2rad(angle_step * i)) * (CLOCK_RADIUS - 16 - length);
        int y2 = window->width / 2 + sin(deg2rad(angle_step * i)) * (CLOCK_RADIUS - 16 - length);

        draw_line(window, x1, y1, x2, y2, is_thick ? 2 : 1, COL_BLACK);
    }

    // Remember to add 90° to the final angle, because the numbers on the clock
    // don't start on the center-right of the circle
    double hours_angle = (90 + 360 / 12 * (12 - datetime->hour % 12)) % 360;
    draw_line(window,
        center_x, center_y,
        center_x + cos(deg2rad(hours_angle)) * HOURS_HAND_LENGTH,
        center_y - sin(deg2rad(hours_angle)) * HOURS_HAND_LENGTH,
        4, COL_BLACK
    );

    double minutes_angle = (90 + 360 / 60 * (60 - datetime->minute)) % 360;
    draw_line(window,
        center_x, center_y,
        center_x + cos(deg2rad(minutes_angle)) * MINUTES_HAND_LENGTH,
        center_y - sin(deg2rad(minutes_angle)) * MINUTES_HAND_LENGTH,
        2, COL_BLACK
    );

    double seconds_angle = (90 + 360 / 60 * (60 - datetime->second)) % 360;
    draw_line(window,
        center_x, center_y,
        center_x + cos(deg2rad(seconds_angle)) * SECONDS_HAND_LENGTH,
        center_y - sin(deg2rad(seconds_angle)) * SECONDS_HAND_LENGTH,
        1, COL_BLACK
    );

    char text[80];
    sprintf(text, "The current date is: %d/%d/%d", datetime->day, datetime->month, datetime->year - 2000);
    draw_text(window, get_default_font(), text, 6, 8, COL_BLACK);
    printf("%s\n", text);

    refresh_window(window);
}

int main(int argc, char **argv)
{
    Window window = open_window("Clock", 240, 240);
    DateTime datetime;
    get_datetime(&datetime);

    printf("Current time: %d:%d:%d\n", datetime.hour, datetime.minute, datetime.second);
    while (true) {
        draw_clock(&window, &datetime);
        sleep(1000);
        datetime = datetime_add(datetime, 0, 0, 1);
    }

    return 0;
}
