add_executable(
  mandelbrot
  mandelbrot.cpp
  mandelf.c
)

# Pull in pico libraries that we need
target_link_libraries(mandelbrot pico_stdlib pico_multicore pico_dvhstx)
pico_enable_stdio_usb(mandelbrot 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(mandelbrot)
