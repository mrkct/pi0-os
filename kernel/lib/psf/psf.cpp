#include <kernel/kprintf.h>
#include <kernel/lib/memory.h>
#include <kernel/lib/psf/psf.h>

using kernel::Error;
using kernel::GenericErrorCode;

namespace klib {

extern "C" uint8_t __resource_psf_font[];
extern "C" size_t __resource_psf_font_size;

kernel::Error psf_load_default(PSFFont& font)
{
    return psf_load(font, (uint8_t const*)__resource_psf_font, __resource_psf_font_size);
}

kernel::Error psf_load(PSFFont& font, uint8_t const* data, size_t size)
{
    if (size < sizeof(font.header))
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Not a valid PSF2 font (too small)",
            .extra_data = nullptr
        };

    kmemcpy(&font.header, data, sizeof(font.header));
    if (font.header.magic != 0x864ab572)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Not a valid PSF2 font (header magic is wrong)",
            .extra_data = nullptr
        };

    font.data = data;
    font.size = size;

    return kernel::Success;
}

kernel::Error psf_draw_char(PSFFont& font, kernel::Framebuffer& fb, char c, uint32_t color, size_t x, size_t y)
{
    uint8_t const* start_of_glyph = &font.data[sizeof(font.header) + font.header.bytes_per_glyph * c];

    size_t glyph_width = x + 8 < fb.width ? 8 : fb.width - x;
    size_t glyph_height = y + font.header.height < fb.height ? font.header.height : fb.height - y;

    for (size_t glyph_y = 0; glyph_y < glyph_height; glyph_y++) {
        uint8_t glyph_row = start_of_glyph[glyph_y];
        for (size_t glyph_x = 0; glyph_x < glyph_width; glyph_x++) {
            if (glyph_row & (1 << (7 - glyph_x)))
                fb.address[(y + glyph_y) * fb.width + x + glyph_x] = color;
        }
    }

    return kernel::Success;
}

}
