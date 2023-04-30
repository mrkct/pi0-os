#pragma once

#include <kernel/device/videocore.h>
#include <kernel/error.h>
#include <stdint.h>

namespace klib {

struct PSFFont {
    struct {
        uint32_t magic;
        uint32_t version;
        uint32_t header_size;
        uint32_t has_unicode_table;
        uint32_t num_glyphs;
        uint32_t bytes_per_glyph;
        uint32_t height;
        uint32_t width;
    } header;

    uint8_t const* data;
    size_t size;
};

kernel::Error psf_load(PSFFont&, uint8_t const* data, size_t size);

kernel::Error psf_load_default(PSFFont&);

kernel::Error psf_draw_char(PSFFont&, kernel::Framebuffer&, char c, uint32_t color, size_t x, size_t y);

}
