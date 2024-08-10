#include "pico_graphics_dvhstx.hpp"

#ifndef MICROPY_BUILD_TYPE
#define mp_printf(_, ...) printf(__VA_ARGS__);
#else
extern "C" {
#include "py/runtime.h"
}
#endif

namespace pimoroni {
    RGB565 to_rgb565_noswap(const RGB& color) {
        uint16_t p = ((color.r & 0b11111000) << 8) |
                     ((color.g & 0b11111100) << 3) |
                     ((color.b & 0b11111000) >> 3);

        return p;
    }

    PicoGraphics_PenDVHSTX_RGB565::PicoGraphics_PenDVHSTX_RGB565(uint16_t width, uint16_t height, DVHSTX &dv_display)
      : PicoGraphicsDVHSTX(width, height, dv_display)
    {
        this->pen_type = PEN_DV_RGB555;
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_pen(uint c) {
        color = c;
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_bg(uint c) {
        background = c;
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_depth(uint8_t new_depth) {
        depth = new_depth > 0 ? 0x8000 : 0;
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_pen(uint8_t r, uint8_t g, uint8_t b) {
        RGB src_color{r, g, b};
        color = to_rgb565_noswap(src_color); 
    }
    int PicoGraphics_PenDVHSTX_RGB565::create_pen(uint8_t r, uint8_t g, uint8_t b) {
        return to_rgb565_noswap(RGB(r, g, b));
    }
    int PicoGraphics_PenDVHSTX_RGB565::create_pen_hsv(float h, float s, float v) {
        return to_rgb565_noswap(RGB::from_hsv(h, s, v));
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_pixel(const Point &p) {
        driver.write_pixel(p, color);
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_pixel_span(const Point &p, uint l) {
        driver.write_pixel_span(p, l, color);
    }
    void PicoGraphics_PenDVHSTX_RGB565::set_pixel_alpha(const Point &p, const uint8_t a) {
        uint16_t src = background;
        if (blend_mode == BlendMode::TARGET) {
            driver.read_pixel_span(p, 1, &src);
        }

        uint8_t src_r = (src >> 8) & 0b11111000;
        uint8_t src_g = (src >> 3) & 0b11111100;
        uint8_t src_b = (src << 3) & 0b11111000;

        uint8_t dst_r = (color >> 8) & 0b11111000;
        uint8_t dst_g = (color >> 3) & 0b11111100;
        uint8_t dst_b = (color << 3) & 0b11111000;

        RGB565 blended = to_rgb565_noswap(RGB(src_r, src_g, src_b).blend(RGB(dst_r, dst_g, dst_b), a));

        driver.write_pixel(p, blended);
    }

    bool PicoGraphics_PenDVHSTX_RGB565::render_pico_vector_tile(const Rect &src_bounds, uint8_t* alpha_data, uint32_t stride, uint8_t alpha_type) {
        return false;
    }
}