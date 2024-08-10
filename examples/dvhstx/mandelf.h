#pragma once

typedef struct {
  // Configuration
  int16_t rows;
  int16_t cols;

  uint16_t max_iter;
  uint16_t iter_offset;
  float minx, miny, maxx, maxy;
  bool use_cycle_check;

  // State
  volatile bool done;
  volatile uint16_t min_iter;
  float incx, incy;
  volatile uint32_t count_inside;
} FractalBuffer;

// Generate a section of the fractal into buff
// Result written to buff is 0 for inside Mandelbrot set
// Otherwise iteration of escape minus min_iter (clamped to 1)
void init_fractal(FractalBuffer* fractal);
void generate_one_line(FractalBuffer* f, uint8_t* buf, uint16_t row);
