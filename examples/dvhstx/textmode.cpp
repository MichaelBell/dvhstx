#include <stdio.h>
#include "hardware/uart.h"
#include "drivers/dvhstx/dvhstx.hpp"

using namespace pimoroni;

#define FRAME_WIDTH 91
#define FRAME_HEIGHT 30

static DVHSTX display;

using namespace pimoroni;

void fill_text(int frame)
{
    char buf[128];
    sprintf(buf, "Hello World!  Frame: %d", frame);
    display.write_text({0,0}, buf);

    for (int i = 0; i < 0x7f; ++i) {
      buf[i] = i;
    }
    buf[0x7f] = 0;

    for (int i = 1; i < FRAME_HEIGHT; ++i) {
      display.write_text({0, i}, &buf[0x20 + i], (i & 1) ? (i & 2) ? DVHSTX::TEXT_WHITE : DVHSTX::TEXT_BLUE : (i & 2) ? DVHSTX::TEXT_YELLOW : DVHSTX::TEXT_RED);
    }
}

int main()
{
    stdio_init_all();

    printf("Main\n");

    bool rv = display.init(FRAME_WIDTH, FRAME_HEIGHT, DVHSTX::MODE_TEXT_RGB111);

    printf("Init complete: %s", rv ? "True" : "False");

    fill_text(0);
    display.flip_now();

    int frame = 1;
    while(1) {
      fill_text(frame++);
      display.flip_blocking();
      printf(".\n");
    }
}
