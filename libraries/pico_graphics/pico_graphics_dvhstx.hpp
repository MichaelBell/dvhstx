#include "pico_graphics.hpp"
#include "drivers/dvhstx/dvhstx.hpp"

namespace pimoroni {
  enum BlendMode {
    TARGET = 0,
    FIXED = 1,
  };

  class PicoGraphicsDVHSTX : public PicoGraphics {
    public:
      DVHSTX &driver;
      BlendMode blend_mode = BlendMode::TARGET;

      void set_blend_mode(BlendMode mode) {
        blend_mode = mode;
      }

      virtual void set_depth(uint8_t new_depth) {}
      virtual void set_bg(uint c) {};

      PicoGraphicsDVHSTX(uint16_t width, uint16_t height, DVHSTX &dv_display)
      : PicoGraphics(width, height, nullptr),
        driver(dv_display)
      {
          this->pen_type = PEN_DV_RGB555;
      }
  };

  class PicoGraphics_PenDVHSTX_RGB565 : public PicoGraphicsDVHSTX {
    public:
      RGB565 color;
      RGB565 background;
      uint16_t depth = 0;

      PicoGraphics_PenDVHSTX_RGB565(uint16_t width, uint16_t height, DVHSTX &dv_display);
      void set_pen(uint c) override;
      void set_bg(uint c) override;
      void set_depth(uint8_t new_depth) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void set_pixel_alpha(const Point &p, const uint8_t a) override;

      bool supports_alpha_blend() override {return true;}

      bool render_tile(const Tile *tile) override;

      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB565);
      }
  };

  class PicoGraphics_PenDVHSTX_P8 : public PicoGraphicsDVHSTX {
    public:
      static const uint16_t palette_size = 256;
      uint8_t color;
      uint8_t depth = 0;
      bool used[palette_size];

      std::array<std::array<uint8_t, 16>, 512> candidate_cache;
      std::array<uint8_t, 16> candidates;
      bool cache_built = false;

      PicoGraphics_PenDVHSTX_P8(uint16_t width, uint16_t height, DVHSTX &dv_display);
      void set_pen(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      void set_depth(uint8_t new_depth) override;
      int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      int reset_pen(uint8_t i) override;

      int get_palette_size() override {return 0;};
      RGB* get_palette() override {return nullptr;};

      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void get_dither_candidates(const RGB &col, const RGB *palette, size_t len, std::array<uint8_t, 16> &candidates);
      void set_pixel_dither(const Point &p, const RGB &c) override;

      static size_t buffer_size(uint w, uint h) {
          return w * h;
      }
  };  
}