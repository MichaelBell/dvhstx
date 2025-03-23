#include <stdio.h>
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "drivers/dvhstx/dvhstx.hpp"
#include "libraries/pico_graphics/pico_graphics_dvhstx.hpp"
#include "libraries/pngdec/PNGdec.h"

using namespace pimoroni;

#if 0
#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

// To upload, e.g.: picotool load ngc2207-1280x720.png -t bin -o 0x10080000
static uint8_t* png_data[] = {
    (uint8_t*)0x10080000,
};
static int png_len[] = {
    1498121,
};
#else
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080

static uint8_t* png_data[] = {
    (uint8_t*)0x10080000,
};
static int png_len[] = {
    3629086,
};
#endif

static DVHSTX display;
static PicoGraphics_PenDVHSTX_RGB565 graphics(FRAME_WIDTH, FRAME_HEIGHT, display);

void core1_main() {
    display.init(FRAME_WIDTH, FRAME_HEIGHT, DVHSTX::MODE_RGB888, DVHSTX::Pinout{13, 15, 17, 19}, DVHSTX::MEM_SINGLE_APS6408);

    multicore_fifo_push_blocking(1);

    while (1) __wfe();
}

static PNG png;
static uint32_t line_buffer[FRAME_WIDTH];
void png_draw(PNGDRAW* pDraw) {
    uint8_t* pData = pDraw->pPixels;
    Point p(0, pDraw->y);
    if (pDraw->y == 0) printf("Bpp %d pixel type %d, width %d\n", pDraw->iBpp, pDraw->iPixelType, pDraw->iWidth);
    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        display.write_pixel32_span(p, pDraw->iWidth, (uint32_t*)pData);
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR) {
        uint8_t* pPaddedData = (uint8_t*)line_buffer;
        for (int j = 0; j < pDraw->iWidth; ++j) {
            *pPaddedData++ = pData[2];
            *pPaddedData++ = pData[1];
            *pPaddedData++ = pData[0];
            *pPaddedData++ = 0;
            pData += 3;
        }
        display.write_pixel32_span(p, pDraw->iWidth, line_buffer);
    }
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

    const int num_images = sizeof(png_len) / sizeof(png_len[0]);
    int i = 0;

    while (true) {
        display.flip_blocking();
        display.set_blank(true);

        png.openRAM(png_data[i], png_len[i], png_draw);
        png.decode(nullptr, 0);

        const float temperature = read_onboard_temperature();
        printf("Decoded, Temp = %.02fC\n", temperature);

        if (++i == num_images) i = 0;

        display.set_blank(false);
        sleep_ms(8000);
    }

    return 0;
}