#include <stdio.h>
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "drivers/dvhstx/dvhstx.hpp"
#include "libraries/pico_graphics/pico_graphics_dvhstx.hpp"
#include "libraries/jpegdec/JPEGDEC.h"

using namespace pimoroni;

#if 0
#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

// To upload, e.g.: picotool load ngc4900-1280x720.jpg -t bin -o 0x10080000
static uint8_t* jpeg_data[] = {
    (uint8_t*)0x10080000,
    (uint8_t*)0x10100000,
};
static int jpeg_len[] = {
    368260,
    304810,
};
#else
#if 1
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080

static uint8_t* jpeg_data[] = {
    (uint8_t*)0x10180000,
    (uint8_t*)0x10280000,
};
static int jpeg_len[] = {
    741203,
    883488,
};
#else
#define FRAME_WIDTH 2560
#define FRAME_HEIGHT 1440

static uint8_t* jpeg_data[] = {
    (uint8_t*)0x10180000,
    (uint8_t*)0x10280000,
};
static int jpeg_len[] = {
    1001390,
    998808,
};
#endif
#endif

static DVHSTX display;
static PicoGraphics_PenDVHSTX_RGB565 graphics(FRAME_WIDTH, FRAME_HEIGHT, display);

void core1_main() {
    display.init(FRAME_WIDTH, FRAME_HEIGHT, DVHSTX::MODE_RGB565, DVHSTX::Pinout{13, 15, 17, 19}, DVHSTX::MEM_SINGLE_APS6408);

    multicore_fifo_push_blocking(1);

    while (1) __wfe();
}

static JPEGDEC jpeg;
int jpeg_draw(JPEGDRAW* pDraw) {
    for (int i = 0; i < pDraw->iHeight; ++i) {
        uint16_t* pData = &pDraw->pPixels[i * pDraw->iWidth];
        Point p(pDraw->x, pDraw->y + i);
        display.write_pixel_span(p, pDraw->iWidthUsed, pData);
    }

    return 1;
}

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float read_onboard_temperature() {
    
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return tempC;
}

int main() {
    stdio_init_all();
    //while (!stdio_usb_connected());

    display.set_blank(true);

    /* Initialize hardware AD converter, enable onboard temperature sensor and
     *   select its channel (do this once for efficiency, but beware that this
     *   is a global operation). */
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    multicore_launch_core1(core1_main);
    multicore_fifo_pop_blocking();

    const int num_images = sizeof(jpeg_len) / sizeof(jpeg_len[0]);
    int i = 0;

    while (true) {
        display.flip_blocking();
        display.set_blank(true);

        jpeg.openRAM(jpeg_data[i], jpeg_len[i], jpeg_draw);
        jpeg.decode(0, 0, 0);

        const float temperature = read_onboard_temperature();
        printf("Decoded, Temp = %.02fC\n", temperature);

        if (++i == num_images) i = 0;

        display.set_blank(false);
        sleep_ms(8000);
    }

    return 0;
}