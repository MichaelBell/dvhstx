#pragma once

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "common/pimoroni_common.hpp"
#include "common/pimoroni_i2c.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

// DVI HSTX driver for use with Pimoroni PicoGraphics

namespace pimoroni {

  // Digital Video using HSTX
  // Valid screen modes are:
  //   Pixel doubled: 640x480 (60Hz), 720x480 (60Hz), 720x400 (70Hz), 720x576 (50Hz), 
  //                  800x600 (60Hz), 800x480 (60Hz), 800x450 (60Hz), 960x540 (60Hz), 1024x768 (60Hz)
  //   Pixel doubled or quadrupled: 1280x720 (50Hz)
  //
  // Giving valid resolutions:
  //   320x180, 640x360 (well supported, square pixels on a 16:9 display)
  //   480x270, 400x225 (sometimes supported, square pixels on a 16:9 display)
  //   320x240, 360x240, 360x200, 360x288, 400x300, 512x384 (well supported, but pixels aren't square)
  //   400x240 (sometimes supported, pixels aren't square)
  //
  // Note that the double buffer is in RAM, so 640x360 uses almost all of the available RAM.
  class DVHSTX {
  public:
    static constexpr int PALETTE_SIZE = 256;

    enum Mode {
      MODE_PALETTE = 2,
      MODE_RGB565 = 1,
      MODE_RGB888 = 3,
      MODE_TEXT_MONO = 4,
      MODE_TEXT_RGB111 = 5,
    };

    enum TextColour {
      TEXT_BLACK   = 0,
      TEXT_RED     = 0b1000000,
      TEXT_GREEN   = 0b0001000,
      TEXT_BLUE    = 0b0000001,
      TEXT_YELLOW  = 0b1001000,
      TEXT_MAGENTA = 0b1000001,
      TEXT_CYAN    = 0b0001001,
      TEXT_WHITE   = 0b1001001,
    };    

    //--------------------------------------------------
    // Variables
    //--------------------------------------------------
  protected:
    friend void vsync_callback();

    uint16_t display_width = 320;
    uint16_t display_height = 180;
    uint16_t frame_width = 320;
    uint16_t frame_height = 180;
    uint8_t frame_bytes_per_pixel = 2;
    uint8_t bank = 0;
    uint8_t h_repeat = 4;
    uint8_t v_repeat = 4;
    Mode mode = MODE_RGB565;

  public:
    DVHSTX()
    {}

    //--------------------------------------------------
    // Methods
    //--------------------------------------------------
    public:
      // 16bpp interface
      void write_pixel(const Point &p, uint16_t colour);
      void write_pixel_span(const Point &p, uint l, uint16_t colour);
      void write_pixel_span(const Point &p, uint l, uint16_t *data);
      void read_pixel_span(const Point &p, uint l, uint16_t *data);

      // 256 colour palette mode.
      void set_palette(RGB888 new_palette[PALETTE_SIZE]);
      void set_palette_colour(uint8_t entry, RGB888 colour);
      RGB888* get_palette();

      void write_palette_pixel(const Point &p, uint8_t colour);
      void write_palette_pixel_span(const Point &p, uint l, uint8_t colour);
      void write_palette_pixel_span(const Point &p, uint l, uint8_t* data);
      void read_palette_pixel_span(const Point &p, uint l, uint8_t *data);

      // Text mode (91 x 30)
      // Immediate writes to the active buffer instead of the back buffer
      void write_text(const Point &p, const char* text, TextColour colour = TEXT_WHITE, bool immediate = false);

      void clear();

      bool init(uint16_t width, uint16_t height, Mode mode = MODE_RGB565);

      // Wait for vsync and then flip the buffers
      void flip_blocking();

      // Flip immediately without waiting for vsync
      void flip_now();

      void wait_for_vsync();

      // flip_async queues a flip to happen next vsync but returns without blocking.
      // You should call wait_for_flip before doing any more reads or writes, defining sprites, etc.
      void flip_async();
      void wait_for_flip();

      // DMA handlers, should not be called externally
      void gfx_dma_handler();
      void text_dma_handler();

    private:
      RGB888 palette[PALETTE_SIZE];

      uint8_t* frame_buffer_display;
      uint8_t* frame_buffer_back;
      uint32_t* font_cache;

      uint16_t* point_to_ptr16(const Point &p) const {
        return ((uint16_t*)frame_buffer_back) + (p.y * (uint32_t)frame_width) + p.x;
      }

      uint8_t* point_to_ptr_palette(const Point &p) const {
        return frame_buffer_back + (p.y * (uint32_t)frame_width) + p.x;
      }

      uint8_t* point_to_ptr_text(const Point &p, bool immediate) const {
        const uint32_t offset = (p.y * (uint32_t)frame_width) + p.x;
        if (immediate) return frame_buffer_display + offset;
        return frame_buffer_back + offset;
      }

      void display_setup_clock();

      // DMA scanline filling
      uint ch_num = 0;
      int line_num = -1;

      volatile int v_scanline = 2;
      volatile bool flip_next;

      uint32_t* line_buffers;
      const struct dvi_timing* timing_mode;
      int v_inactive_total;
      int v_total_active_lines;

      uint h_repeat_shift;
      uint v_repeat_shift;
      int line_bytes_per_pixel;

      uint32_t* display_palette = nullptr;
  };
}
