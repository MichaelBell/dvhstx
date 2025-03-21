add_executable(
  mandelbrot
  mandelbrot.cpp
  mandelf.c
)

# Pull in pico libraries that we need
target_link_libraries(mandelbrot pico_stdlib pico_multicore pico_dvhstx hardware_adc)
pico_enable_stdio_usb(mandelbrot 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(mandelbrot)

add_executable(
  textmode
  textmode.cpp
)

# Pull in pico libraries that we need
target_link_libraries(textmode pico_stdlib pico_dvhstx)
pico_enable_stdio_usb(textmode 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(textmode)
