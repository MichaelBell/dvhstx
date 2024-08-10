#include <stdio.h>
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "drivers/dvhstx/dvhstx.hpp"
#include "libraries/pico_graphics/pico_graphics_dvhstx.hpp"

extern "C" {
#include "mandelf.h"
}

using namespace pimoroni;

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 360

//#define FRAME_WIDTH 400
//#define FRAME_HEIGHT 300

//#define FRAME_WIDTH 320
//#define FRAME_HEIGHT 180

static DVHSTX display;
static PicoGraphics_PenDVHSTX_P8 graphics(FRAME_WIDTH, FRAME_HEIGHT, display);

static FractalBuffer fractal;

static void init_palette() {
    graphics.create_pen(0, 0, 0);
    for (int i = 0; i < 64; ++i) {
        graphics.create_pen_hsv(i * (1.f / 63.f), 1.0f, 0.5f + (i & 7) * (0.5f / 7.f));
    }
}

static void init_mandel() {
  fractal.rows = FRAME_HEIGHT / 2;
  fractal.cols = FRAME_WIDTH;
  fractal.max_iter = 63;
  fractal.iter_offset = 0;
  fractal.minx = -2.25f;
  fractal.maxx = 0.75f;
  fractal.miny = -1.6f;
  fractal.maxy = 0.f - (1.6f / FRAME_HEIGHT); // Half a row
  fractal.use_cycle_check = true;
  init_fractal(&fractal);
}

#define NUM_ZOOMS 100
static uint32_t zoom_count = 0;

static void zoom_mandel() {
  if (++zoom_count == NUM_ZOOMS)
  {
    init_mandel();
    zoom_count = 0;
    sleep_ms(2000);
    return;
  }

  float zoomx = -.75f - .7f * ((float)zoom_count / (float)NUM_ZOOMS);
  float sizex = fractal.maxx - fractal.minx;
  float sizey = fractal.miny * -2.f;
  float zoomr = 0.974f * 0.5f;
  fractal.minx = zoomx - zoomr * sizex;
  fractal.maxx = zoomx + zoomr * sizex;
  fractal.miny = -zoomr * sizey;
  fractal.maxy = 0.f + fractal.miny / FRAME_HEIGHT;
  init_fractal(&fractal);
}

static void display_row(int y, uint8_t* buf) {
    display.write_palette_pixel_span({0, y}, FRAME_WIDTH, buf);
    display.write_palette_pixel_span({0, FRAME_HEIGHT - 1 - y}, FRAME_WIDTH, buf);
}

static uint8_t row_buf_core1[FRAME_WIDTH];
void core1_main() {
    while (true) {
        int y = multicore_fifo_pop_blocking();
        generate_one_line(&fractal, row_buf_core1, y);
        multicore_fifo_push_blocking(y);
    }
}

static uint8_t row_buf[FRAME_WIDTH];
static void draw_two_rows(int y) {
    multicore_fifo_push_blocking(y+1);
    generate_one_line(&fractal, row_buf, y);

    display_row(y, row_buf);

    multicore_fifo_pop_blocking();
    display_row(y+1, row_buf_core1);
}

void draw_mandel() {
    //display.wait_for_flip();
    for (int y = 0; y < FRAME_HEIGHT / 2; y += 2)
    {
        draw_two_rows(y);
    }
    //display.flip_async();
    display.flip_now();
}

int main() {
    stdio_init_all();

    display.init(FRAME_WIDTH, FRAME_HEIGHT, DVHSTX::MODE_PALETTE);

    init_palette();

    graphics.set_pen(0);
    graphics.clear();
    display.flip_now();
    graphics.clear();

    multicore_launch_core1(core1_main);

    init_mandel();
    draw_mandel();

    while(true) {
        absolute_time_t start_time = get_absolute_time();
        zoom_mandel();
        draw_mandel();
        printf("Drawing zoom %ld took %.2fms\n", zoom_count, absolute_time_diff_us(start_time, get_absolute_time()) * 0.001f);
    }
}
