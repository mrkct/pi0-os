#include <kernel/device/framebuffer.h>
#include <kernel/lib/math.h>


namespace kernel {

static Framebuffer g_main_framebuffer;


Framebuffer& get_main_framebuffer()
{
    return g_main_framebuffer;
}

void blit_to_main_framebuffer(uint32_t *data, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    auto x1 = max<int32_t>(0, x);
    auto y1 = max<int32_t>(0, y);
    auto x2 = min<int32_t>(g_main_framebuffer.width, x + width);
    auto y2 = min<int32_t>(g_main_framebuffer.height, y + height);

    uint32_t src_width = x2 - x1;
    uint32_t src_height = y2 - y1;

    if (src_width <= 0 || src_height <= 0)
        return;

    for (uint32_t j = 0; j < src_height; ++j) {
        for (uint32_t i = 0; i < src_width; ++i) {
            uint32_t dest_index = (y1 + j) * g_main_framebuffer.width + (x1 + i);
            uint32_t source_index = (y1 - y + j) * width + (x1 - x + i);
            g_main_framebuffer.address[dest_index] = data[source_index];
        }
    }
}


}
