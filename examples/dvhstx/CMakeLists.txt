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
  bubbles
  bubbles.cpp
)

# Pull in pico libraries that we need
target_link_libraries(bubbles pico_stdlib pico_multicore pico_dvhstx)
pico_enable_stdio_usb(bubbles 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(bubbles)

add_executable(
  show_image
  show_image.cpp
)

include(libraries/jpegdec/jpegdec)

# Pull in pico libraries that we need
target_link_libraries(show_image pico_stdlib pico_multicore pico_dvhstx jpegdec hardware_adc)
pico_enable_stdio_usb(show_image 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(show_image)

add_executable(
  show_png
  show_png.cpp
)

include(libraries/pngdec/pngdec)

# Pull in pico libraries that we need
target_link_libraries(show_png pico_stdlib pico_multicore pico_dvhstx pngdec hardware_adc)
pico_enable_stdio_usb(show_png 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(show_png)


add_executable(
  textmode
  textmode.cpp
)

# Pull in pico libraries that we need
target_link_libraries(textmode pico_stdlib pico_dvhstx)
pico_enable_stdio_usb(textmode 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(textmode)
