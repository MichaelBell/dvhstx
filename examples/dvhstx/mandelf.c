// Copyright (C) Michael Bell 2021

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"

#include "mandelf.h"

// Cycle checking parameters
#define MAX_CYCLE_LEN 8          // Must be power of 2
#define MIN_CYCLE_CHECK_ITER 24  // Must be multiple of max cycle len
#define CYCLE_TOLERANCE 4e-3

#define ESCAPE_SQUARE 4.f

void init_fractal(FractalBuffer* f)
{
  f->done = false;
  f->min_iter = f->max_iter - 1;
  f->incx = (f->maxx - f->minx) / (f->cols - 1);
  f->incy = (f->maxy - f->miny) / (f->rows - 1);
  f->count_inside = 0;
}

static inline void generate_one(FractalBuffer* f, float x0, float y0, uint8_t* buffptr)
{
  float x = x0;
  float y = y0;

  uint16_t k = 1;
  for (; k < f->max_iter; ++k) {
    float x_square = x*x;
    float y_square = y*y;
    if (x_square + y_square > ESCAPE_SQUARE) break;

    float nextx = x_square - y_square + x0;
    y = x*y*2.f + y0;
    x = nextx;
  }
  if (k == f->max_iter) {
    *buffptr = 0;
    f->count_inside++;
  } else {
    if (k > f->iter_offset) k -= f->iter_offset;
    else k = 1;
    *buffptr = k;
    if (f->min_iter > k) f->min_iter = k;
  }
}

void generate_one_line(FractalBuffer* f, uint8_t* buf, uint16_t ipos)
{
  if (f->done) return;

  float y0 = f->miny + ipos * f->incy;
  float x0 = f->minx;

  for (int16_t j = 0; j < f->cols; ++j) {
    generate_one(f, x0, y0, buf++);
    x0 += f->incx;
  }
}
