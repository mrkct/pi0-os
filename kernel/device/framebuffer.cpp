#include <kernel/device/framebuffer.h>
#include <kernel/lib/math.h>
#include <kernel/lib/string.h>


namespace kernel {

static Framebuffer g_main_framebuffer;


Framebuffer& get_main_framebuffer()
{
    return g_main_framebuffer;
}

void blit_to_main_framebuffer(uint32_t *data, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    auto target_x1 = max<int32_t>(0, x);
    auto target_y1 = max<int32_t>(0, y);
    auto target_x2 = min<int32_t>(g_main_framebuffer.width, x + width);
    auto target_y2 = min<int32_t>(g_main_framebuffer.height, y + height);

    auto source_x1 = abs(min<int32_t>(x, 0));
    auto source_y1 = abs(min<int32_t>(y, 0));
    auto source_x2 = min<int32_t>(x + width, g_main_framebuffer.width) - x;
    auto source_y2 = min<int32_t>(y + height, g_main_framebuffer.height) - y;

    if (target_x2 - target_x1 <= 0 || target_y2 - target_y1 <= 0)
        return;

    uint32_t *src = &data[source_y1 * height + source_x1];
    uint32_t *dst = (uint32_t*)&(((uint8_t*)g_main_framebuffer.address)[target_y1 * g_main_framebuffer.pitch + target_x1*sizeof(uint32_t)]);

    auto lines_to_copy = target_y2 - target_y1;
    auto bytes_in_a_line = (target_x2 - target_x1) * sizeof(uint32_t);
    for (; lines_to_copy > 0; lines_to_copy--) {
        memcpy(dst, src, bytes_in_a_line);
        src += width;
        dst = (uint32_t*) ((uint8_t*) dst + g_main_framebuffer.pitch);
    }
}

}
