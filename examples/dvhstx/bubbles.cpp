#include <stdio.h>
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "drivers/dvhstx/dvhstx.hpp"
#include "libraries/pico_graphics/pico_graphics_dvhstx.hpp"

using namespace pimoroni;

#define FRAME_WIDTH 800
#define FRAME_HEIGHT 600

static DVHSTX display;

static constexpr int NUM_CIRCLES = 50;
static struct Circle {
  uint16_t x, y, size, grow;
  uint32_t pen;
} circles[NUM_CIRCLES];

void setup_pen(PicoGraphicsDVHSTX* graphics, DVHSTX::Mode mode) {
  graphics->create_pen(0, 0, 0);
  graphics->create_pen(0xFF, 0xFF, 0xFF);

  if (mode == DVHSTX::MODE_PALETTE) {
    for (int i = 0; i < 25; ++i) {
      graphics->create_pen_hsv(i * 0.04f, 1.0f, 1.0f);
    }
    for (int i = 0; i < 5; ++i) {
      graphics->create_pen((i+3) * (255/8), 255, 255);
    }
  }

  for(int i =0 ; i < 50 ; i++)
  {
    circles[i].size = (rand() % 50) + 1;
    circles[i].grow = std::max(0, (rand() % 50) - 25);
    circles[i].x = rand() % graphics->bounds.w;
    circles[i].y = rand() % graphics->bounds.h;
    if (mode == DVHSTX::MODE_PALETTE) {
      circles[i].pen = 2 + (i >> 1);
    } else {
      circles[i].pen = graphics->create_pen_hsv(i * 0.02f, 1.0f, 1.0f);
    }
  }
}

PicoGraphicsDVHSTX* graphics;
DVHSTX::Mode mode;

void core1_main() {
#if USE_PALETTE
  mode = DVHSTX::MODE_PALETTE;
  display.init(FRAME_WIDTH, FRAME_HEIGHT, mode, DVHSTX::Pinout{13, 15, 17, 19}, DVHSTX::MEM_DOUBLE_APS6404);
  graphics = new PicoGraphics_PenDVHSTX_P8(FRAME_WIDTH, FRAME_HEIGHT, display);
#else
  mode = DVHSTX::MODE_RGB565;
  display.init(FRAME_WIDTH, FRAME_HEIGHT, mode, DVHSTX::Pinout{13, 15, 17, 19}, DVHSTX::MEM_DOUBLE_APS6404);
  graphics = new PicoGraphics_PenDVHSTX_RGB565(FRAME_WIDTH, FRAME_HEIGHT, display);
#endif

    multicore_fifo_push_blocking(1);

    while (1) __wfe();
}

int main() {
  stdio_init_all();
  //while (!stdio_usb_connected());

  multicore_launch_core1(core1_main);
  multicore_fifo_pop_blocking();

  setup_pen(graphics, mode);

  printf("Starting\n");
  graphics->set_font("bitmap8");

  int frames = 0;
  while (true) {
    uint32_t render_start_time = time_us_32();

#if 1
    for (int j = 0; j < FRAME_HEIGHT; ++j) {
      graphics->set_pen(j & 0xFF, 0xFF, 0xFF);
      graphics->pixel_span({0,j}, FRAME_WIDTH);
    }
#else
    graphics->set_pen(0xFF, 0xFF, 0xFF);
    graphics->clear();
#endif

#if 0
    for (uint i = 0; i < 128; i++) {
      for (uint j = 0; j < 256; j++) {
        RGB555 col = (j << 7) | i;
        graphics->set_pen((col << 3) & 0xF8, (col >> 2) & 0xF8, (col >> 7) & 0xF8);
        graphics->pixel(Point(j, i));
      }
    }

    for (uint i = 0; i < 128; i++) {
      for (uint j = 0; j < 256; j++) {
        graphics->set_pen((j << 7) | i);
        graphics->pixel(Point(i, j+128));
      }
    }
#endif

    for(int i = 0; i < NUM_CIRCLES ; i++)
    {
      graphics->set_pen(0, 0, 0);
      graphics->circle(Point(circles[i].x, circles[i].y), circles[i].size);

      //RGB col = RGB::from_hsv(i * 0.02f, 1.0f, 1.0f);
      //graphics->set_pen(col.r, col.g, col.b);
      graphics->set_pen(circles[i].pen);
      graphics->circle(Point(circles[i].x, circles[i].y), circles[i].size-2);
      if (circles[i].grow) {
        circles[i].size++;
        circles[i].grow--;
      } else {
        circles[i].size--;
        if (circles[i].size == 0) {
          circles[i].size = 1;
          circles[i].grow = rand() % 75;
          circles[i].x = rand() % graphics->bounds.w;
          circles[i].y = rand() % graphics->bounds.h;
        }
      }
    }

#if 0
    uint x = 260; //rand() % graphics->bounds.w;
    uint y = 468; //rand() % graphics->bounds.h;
    printf("Circle at (%d, %d)\n", x, y);
    graphics->set_pen(0);
    graphics->circle(Point(x, y), 25);
  #endif

    uint32_t render_time = time_us_32() - render_start_time;

#if 0
    char buffer[8];
    sprintf(buffer, "%s", 
            gpio_get(BUTTON_Y) == 0 ? "Y" : " ");
    graphics->set_pen(0, 0, 0);
    graphics->text(buffer, {500,10}, FRAME_WIDTH - 500, 3);
#endif

    uint32_t flip_start_time = time_us_32();
    display.flip_blocking();
    uint32_t flip_time = time_us_32() - flip_start_time;
    printf("Render: %.3f, flip: %.3f\n", render_time / 1000.f, flip_time / 1000.f);

    //printf("%02x %02x\n", display.get_gpio(), display.get_gpio_hi());

#if 0
    display.setup_scroll_group(scroll1, 1);
    display.setup_scroll_group(scroll2, 2);
    scroll1.x += scroll_dir[0];
    if (scroll1.x + DISPLAY_WIDTH > FRAME_WIDTH || scroll1.x < 0) {
      scroll_dir[0] = -scroll_dir[0];
      scroll1.x += scroll_dir[0];
    }
    scroll2.y += scroll_dir[1];
    if (scroll2.y + DISPLAY_HEIGHT > FRAME_HEIGHT || scroll2.y < 0) {
      scroll_dir[1] = -scroll_dir[1];
      scroll2.y += scroll_dir[1];
    }
#endif

    ++frames;
  }
}