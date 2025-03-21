#include <string.h>
#include <pico/stdlib.h>

extern "C" {
#include <pico/lock_core.h>
}

#include <algorithm>
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"

#include "hardware/structs/ioqspi.h"
#include "hardware/vreg.h"
#include "hardware/structs/qmi.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"

#include "dvi.hpp"
#include "dvhstx.hpp"

using namespace pimoroni;

#ifdef MICROPY_BUILD_TYPE
#define FRAME_BUFFER_SIZE (640*360)
__attribute__((section(".uninitialized_data"))) static uint8_t frame_buffer_a[FRAME_BUFFER_SIZE];
__attribute__((section(".uninitialized_data"))) static uint8_t frame_buffer_b[FRAME_BUFFER_SIZE];
#endif

#include "font.h"

// If changing the font, note this code will not handle glyphs wider than 13 pixels
#define FONT (&intel_one_mono)

// The first displayable character in text mode is FIRST_GLYPH, and the total
// number of glyphs is GLYPH_COUNT. This makes the last displayable character
// (FIRST_GLYPH + GLYPH_COUNT - 1) or 126, ASCII tilde.
//
// Because all glyphs not present in the cache are displayed as blank, there's
// no need to include space among the cached glyphs.
constexpr int FIRST_GLYPH=33, GLYPH_COUNT=95;

#ifdef MICROPY_BUILD_TYPE
extern "C" {
void dvhstx_debug(const char *fmt, ...);
}
#else
#define dvhstx_debug printf
#endif

static inline __attribute__((always_inline)) uint32_t render_char_line(int c, int y) {
    if (c < 0x20 || c > 0x7e) return 0;
    const lv_font_fmt_txt_glyph_dsc_t* g = &FONT->dsc->glyph_dsc[c - 0x20 + 1];
    const uint8_t *b = FONT->dsc->glyph_bitmap + g->bitmap_index;
    const int ey = y - FONT_HEIGHT + FONT->base_line + g->ofs_y + g->box_h;
    if (ey < 0 || ey >= g->box_h || g->box_w == 0) {
        return 0;
    }
    else {
        int bi = (g->box_w * ey);

        uint32_t bits = (b[bi >> 2] << 24) | (b[(bi >> 2) + 1] << 16) | (b[(bi >> 2) + 2] << 8) | b[(bi >> 2) + 3];
        bits >>= 6 - ((bi & 3) << 1);
        bits &= 0x3ffffff & (0x3ffffff << ((13 - g->box_w) << 1));
        bits >>= g->ofs_x << 1;

        return bits;
    }
}

// ----------------------------------------------------------------------------
// HSTX command lists

// Lists are padded with NOPs to be >= HSTX FIFO size, to avoid DMA rapidly
// pingponging and tripping up the IRQs.

static const uint32_t vblank_line_vsync_off_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1
};
static uint32_t vblank_line_vsync_off[count_of(vblank_line_vsync_off_src)];

static const uint32_t vblank_line_vsync_on_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H1
};
static uint32_t vblank_line_vsync_on[count_of(vblank_line_vsync_on_src)];

static const uint32_t vactive_line_header_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_TMDS      
};
static uint32_t vactive_line_header[count_of(vactive_line_header_src)];

static const uint32_t vactive_text_line_header_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW | 6,
    BLACK_PIXEL_A,
    BLACK_PIXEL_B,
    BLACK_PIXEL_A,
    BLACK_PIXEL_B,
    BLACK_PIXEL_A,
    BLACK_PIXEL_B,
    HSTX_CMD_TMDS
};
static uint32_t vactive_text_line_header[count_of(vactive_text_line_header_src)];

#define NUM_FRAME_LINES 2
#define NUM_CHANS 3

static DVHSTX* display = nullptr;

// ----------------------------------------------------------------------------
// DMA logic

void __scratch_x("display") dma_irq_handler() {
    display->gfx_dma_handler();
}

void __scratch_x("display") DVHSTX::gfx_dma_handler() {
    // ch_num indicates the channel that just finished, which is the one
    // we're about to reload.
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    if (++ch_num == NUM_CHANS) ch_num = 0;

    if (v_scanline >= timing_mode->v_front_porch && v_scanline < (timing_mode->v_front_porch + timing_mode->v_sync_width)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < v_inactive_total) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else {
        const int y = (v_scanline - v_inactive_total) >> v_repeat_shift;
        const int new_line_num = (v_repeat_shift == 0) ? ch_num : (y & (NUM_FRAME_LINES - 1));
        const uint line_buf_total_len = ((timing_mode->h_active_pixels * line_bytes_per_pixel) >> 2) + count_of(vactive_line_header);

        ch->read_addr = (uintptr_t)&line_buffers[new_line_num * line_buf_total_len];
        ch->transfer_count = line_buf_total_len;

        // Fill line buffer
        if (line_num != new_line_num)
        {
            line_num = new_line_num;
            uint32_t* dst_ptr = &line_buffers[line_num * line_buf_total_len + count_of(vactive_line_header)];

            if (line_bytes_per_pixel == 2) {
                uint16_t* src_ptr = (uint16_t*)&frame_buffer_display[y * 2 * (timing_mode->h_active_pixels >> h_repeat_shift)];
                if (h_repeat_shift == 2) {
                    for (int i = 0; i < timing_mode->h_active_pixels >> 1; i += 2) {
                        uint32_t val = (uint32_t)(*src_ptr++) * 0x10001;
                        *dst_ptr++ = val;
                        *dst_ptr++ = val;
                    }
                }
                else {
                    for (int i = 0; i < timing_mode->h_active_pixels >> 1; ++i) {
                        uint32_t val = (uint32_t)(*src_ptr++) * 0x10001;
                        *dst_ptr++ = val;
                    }
                }
            }
            else if (line_bytes_per_pixel == 1) {
                uint8_t* src_ptr = &frame_buffer_display[y * (timing_mode->h_active_pixels >> h_repeat_shift)];
                if (h_repeat_shift == 2) {
                    for (int i = 0; i < timing_mode->h_active_pixels >> 2; ++i) {
                        uint32_t val = (uint32_t)(*src_ptr++) * 0x01010101;
                        *dst_ptr++ = val;
                    }                
                }
                else {
                    for (int i = 0; i < timing_mode->h_active_pixels >> 2; ++i) {
                        uint32_t val = ((uint32_t)(*src_ptr++) * 0x0101);
                        val |= ((uint32_t)(*src_ptr++) * 0x01010000);
                        *dst_ptr++ = val;
                    }
                }
            }
            else if (line_bytes_per_pixel == 4) {
                uint8_t* src_ptr = &frame_buffer_display[y * (timing_mode->h_active_pixels >> h_repeat_shift)];
                if (h_repeat_shift == 2) {
                    for (int i = 0; i < timing_mode->h_active_pixels; i += 4) {
                        uint32_t val = display_palette[*src_ptr++];
                        *dst_ptr++ = val;
                        *dst_ptr++ = val;
                        *dst_ptr++ = val;
                        *dst_ptr++ = val;
                    }
                }
                else {
                    for (int i = 0; i < timing_mode->h_active_pixels; i += 2) {
                        uint32_t val = display_palette[*src_ptr++];
                        *dst_ptr++ = val;
                        *dst_ptr++ = val;
                    }
                }
            }
        }
    }

    if (++v_scanline == v_total_active_lines) {
        v_scanline = 0;
        line_num = -1;
        if (flip_next) {
            flip_next = false;
            display->flip_now();
        }
        __sev();
    }
}

void __scratch_x("display") dma_irq_handler_text() {
    display->text_dma_handler();
}

void __scratch_x("display") DVHSTX::text_dma_handler() {
    // ch_num indicates the channel that just finished, which is the one
    // we're about to reload.
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    if (++ch_num == NUM_CHANS) ch_num = 0;

    if (v_scanline >= timing_mode->v_front_porch && v_scanline < (timing_mode->v_front_porch + timing_mode->v_sync_width)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < v_inactive_total) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else {
        const int y = (v_scanline - v_inactive_total);
        const uint line_buf_total_len = (frame_width * line_bytes_per_pixel + 3) / 4 + count_of(vactive_text_line_header);

        ch->read_addr = (uintptr_t)&line_buffers[ch_num * line_buf_total_len];
        ch->transfer_count = line_buf_total_len;

        // Fill line buffer
        int char_y = y % 24;
        if (line_bytes_per_pixel == 4) {
            uint32_t* dst_ptr = &line_buffers[ch_num * line_buf_total_len + count_of(vactive_text_line_header)];
            uint8_t* src_ptr = &frame_buffer_display[(y / 24) * frame_width];
            for (int i = 0; i < frame_width; ++i) {
                *dst_ptr++ = render_char_line(*src_ptr++, char_y);
            }
        }
        else {
            uint8_t* dst_ptr = (uint8_t*)&line_buffers[ch_num * line_buf_total_len + count_of(vactive_text_line_header)];
            uint8_t* src_ptr = &frame_buffer_display[(y / 24) * frame_width];
            uint8_t* colour_ptr = src_ptr + frame_width * frame_height;
#ifdef __riscv
            for (int i = 0; i < frame_width; ++i) {
                const uint8_t c = (*src_ptr++ - FIRST_GLYPH);
                uint32_t bits = (c < GLYPH_COUNT) ? font_cache[c * 24 + char_y] : 0;
                const uint8_t colour = *colour_ptr++;

                *dst_ptr++ = colour * ((bits >> 24) & 3);
                *dst_ptr++ = colour * ((bits >> 22) & 3);
                *dst_ptr++ = colour * ((bits >> 20) & 3);
                *dst_ptr++ = colour * ((bits >> 18) & 3);
                *dst_ptr++ = colour * ((bits >> 16) & 3);
                *dst_ptr++ = colour * ((bits >> 14) & 3);
                *dst_ptr++ = colour * ((bits >> 12) & 3);
                *dst_ptr++ = colour * ((bits >> 10) & 3);
                *dst_ptr++ = colour * ((bits >> 8) & 3);
                *dst_ptr++ = colour * ((bits >> 6) & 3);
                *dst_ptr++ = colour * ((bits >> 4) & 3);
                *dst_ptr++ = colour * ((bits >> 2) & 3);
                *dst_ptr++ = colour * (bits & 3);
                *dst_ptr++ = 0;
            }
#else
            int i = 0;
            for (; i < frame_width-1; i += 2) {
                uint8_t c = (*src_ptr++ - FIRST_GLYPH);
                uint32_t bits = (c < GLYPH_COUNT) ? font_cache[c * 24 + char_y] : 0;
                uint8_t colour = *colour_ptr++;
                c = (*src_ptr++ - FIRST_GLYPH);
                uint32_t bits2 = (c < GLYPH_COUNT) ? font_cache[c * 24 + char_y] : 0;
                uint8_t colour2 = *colour_ptr++;

                // This ASM works around a compiler bug where the optimizer decides
                // to unroll so hard it spills to the stack.
                uint32_t tmp, tmp2;
                asm volatile (
                    "ubfx %[tmp], %[cbits], #24, #2\n\t"
                    "ubfx %[tmp2], %[cbits], #22, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #20, #2\n\t"
                    "bfi %[tmp], %[tmp2], #16, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #18, #2\n\t"
                    "bfi %[tmp], %[tmp2], #24, #8\n\t"
                    "muls %[tmp], %[colour], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr]]\n\t"

                    "ubfx %[tmp], %[cbits], #16, #2\n\t"
                    "ubfx %[tmp2], %[cbits], #14, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #12, #2\n\t"
                    "bfi %[tmp], %[tmp2], #16, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #10, #2\n\t"
                    "bfi %[tmp], %[tmp2], #24, #8\n\t"
                    "muls %[tmp], %[colour], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr], #4]\n\t"

                    "ubfx %[tmp], %[cbits], #8, #2\n\t"
                    "ubfx %[tmp2], %[cbits], #6, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #4, #2\n\t"
                    "bfi %[tmp], %[tmp2], #16, #8\n\t"
                    "ubfx %[tmp2], %[cbits], #2, #2\n\t"
                    "bfi %[tmp], %[tmp2], #24, #8\n\t"
                    "muls %[tmp], %[colour], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr], #8]\n\t"

                    "ubfx %[tmp], %[cbits2], #24, #2\n\t"
                    "ubfx %[tmp2], %[cbits2], #22, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "muls %[tmp], %[colour2], %[tmp]\n\t"
                    "and %[tmp2], %[cbits], #3\n\t"
                    "muls %[tmp2], %[colour], %[tmp2]\n\t"
                    "bfi %[tmp2], %[tmp], #16, #16\n\t"
                    "str %[tmp2], [%[dst_ptr], #12]\n\t"

                    "ubfx %[tmp], %[cbits2], #20, #2\n\t"
                    "ubfx %[tmp2], %[cbits2], #18, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "ubfx %[tmp2], %[cbits2], #16, #2\n\t"
                    "bfi %[tmp], %[tmp2], #16, #8\n\t"
                    "ubfx %[tmp2], %[cbits2], #14, #2\n\t"
                    "bfi %[tmp], %[tmp2], #24, #8\n\t"
                    "muls %[tmp], %[colour2], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr], #16]\n\t"

                    "ubfx %[tmp], %[cbits2], #12, #2\n\t"
                    "ubfx %[tmp2], %[cbits2], #10, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "ubfx %[tmp2], %[cbits2], #8, #2\n\t"
                    "bfi %[tmp], %[tmp2], #16, #8\n\t"
                    "ubfx %[tmp2], %[cbits2], #6, #2\n\t"
                    "bfi %[tmp], %[tmp2], #24, #8\n\t"
                    "muls %[tmp], %[colour2], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr], #20]\n\t"

                    "ubfx %[tmp], %[cbits2], #4, #2\n\t"
                    "ubfx %[tmp2], %[cbits2], #2, #2\n\t"
                    "bfi %[tmp], %[tmp2], #8, #8\n\t"
                    "bfi %[tmp], %[cbits2], #16, #2\n\t"
                    "muls %[tmp], %[colour2], %[tmp]\n\t"
                    "str %[tmp], [%[dst_ptr], #24]\n\t"
                    : [tmp] "=&l" (tmp),
                      [tmp2] "=&l" (tmp2)
                    : [cbits] "r" (bits),
                      [colour] "l" (colour),
                      [cbits2] "r" (bits2),
                      [colour2] "l" (colour2),
                      [dst_ptr] "r" (dst_ptr)
                    : "cc", "memory" );
                dst_ptr += 14 * 2;                
            }
            if (i != frame_width) {
                const uint8_t c = (*src_ptr++ - FIRST_GLYPH);
                uint32_t bits = (c < GLYPH_COUNT) ? font_cache[c * 24 + char_y] : 0;
                const uint8_t colour = *colour_ptr++;

                *dst_ptr++ = colour * ((bits >> 24) & 3);
                *dst_ptr++ = colour * ((bits >> 22) & 3);
                *dst_ptr++ = colour * ((bits >> 20) & 3);
                *dst_ptr++ = colour * ((bits >> 18) & 3);
                *dst_ptr++ = colour * ((bits >> 16) & 3);
                *dst_ptr++ = colour * ((bits >> 14) & 3);
                *dst_ptr++ = colour * ((bits >> 12) & 3);
                *dst_ptr++ = colour * ((bits >> 10) & 3);
                *dst_ptr++ = colour * ((bits >> 8) & 3);
                *dst_ptr++ = colour * ((bits >> 6) & 3);
                *dst_ptr++ = colour * ((bits >> 4) & 3);
                *dst_ptr++ = colour * ((bits >> 2) & 3);
                *dst_ptr++ = colour * (bits & 3);
                *dst_ptr++ = 0;
            }
#endif
        }
    }

    if (++v_scanline == v_total_active_lines) {
        v_scanline = 0;
        line_num = -1;
        if (flip_next) {
            flip_next = false;
            display->flip_now();
        }
        __sev();
    }
}

// ----------------------------------------------------------------------------
// Experimental clock config

#ifndef MICROPY_BUILD_TYPE
static void __no_inline_not_in_flash_func(set_qmi_timing)() {
    // Make sure flash is deselected - QMI doesn't appear to have a busy flag(!)
    while ((ioqspi_hw->io[1].status & IO_QSPI_GPIO_QSPI_SS_STATUS_OUTTOPAD_BITS) != IO_QSPI_GPIO_QSPI_SS_STATUS_OUTTOPAD_BITS)
        ;

    qmi_hw->m[0].timing = 0x40000202;
    //qmi_hw->m[0].timing = 0x40000101;
    // Force a read through XIP to ensure the timing is applied
    volatile uint32_t* ptr = (volatile uint32_t*)0x14000000;
    (void) *ptr;
}
#endif

extern "C" void __no_inline_not_in_flash_func(display_setup_clock_preinit)() {
    uint32_t intr_stash = save_and_disable_interrupts();

    // Before messing with clock speeds ensure QSPI clock is nice and slow
    hw_write_masked(&qmi_hw->m[0].timing, 6, QMI_M0_TIMING_CLKDIV_BITS);

    // We're going to go fast, boost the voltage a little
    vreg_set_voltage(VREG_VOLTAGE_1_15);

    // Force a read through XIP to ensure the timing is applied before raising the clock rate
    volatile uint32_t* ptr = (volatile uint32_t*)0x14000000;
    (void) *ptr;

    // Before we touch PLLs, switch sys and ref cleanly away from their aux sources.
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 0x1)
        tight_loop_contents();
    hw_write_masked(&clocks_hw->clk[clk_ref].ctrl, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, CLOCKS_CLK_REF_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_ref].selected != 0x4)
        tight_loop_contents();

    // Stop the other clocks so we don't worry about overspeed
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    clock_stop(clk_peri);
    clock_stop(clk_hstx);

    // Set USB PLL to 528MHz
    pll_init(pll_usb, PLL_COMMON_REFDIV, 1584 * MHZ, 3, 1);

    const uint32_t usb_pll_freq = 528 * MHZ;

    // CLK SYS = PLL USB 528MHz / 2 = 264MHz
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    usb_pll_freq, usb_pll_freq / 2);

    // CLK PERI = PLL USB 528MHz / 4 = 132MHz
    clock_configure(clk_peri,
                    0, // Only AUX mux on ADC
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    usb_pll_freq, usb_pll_freq / 4);

    // CLK USB = PLL USB 528MHz / 11 = 48MHz
    clock_configure(clk_usb,
                    0, // No GLMUX
                    CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    usb_pll_freq,
                    USB_CLK_KHZ * KHZ);

    // CLK ADC = PLL USB 528MHz / 11 = 48MHz
    clock_configure(clk_adc,
                    0, // No GLMUX
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    usb_pll_freq,
                    USB_CLK_KHZ * KHZ);

    // Now we are running fast set fast QSPI clock and read delay
    // On MicroPython this is setup by main.
#ifndef MICROPY_BUILD_TYPE
    set_qmi_timing();
#endif

    restore_interrupts(intr_stash);
}

#ifndef MICROPY_BUILD_TYPE
// Trigger clock setup early - on MicroPython this is done by a hook in main.
namespace {
    class DV_preinit {
        public:
        DV_preinit() {
            display_setup_clock_preinit();
        }
    };
    DV_preinit dv_preinit __attribute__ ((init_priority (101))) ;
}
#endif

void DVHSTX::display_setup_clock() {
    const uint32_t dvi_clock_khz = timing_mode->bit_clk_khz >> 1;
    uint vco_freq, post_div1, post_div2;
    if (!check_sys_clock_khz(dvi_clock_khz, &vco_freq, &post_div1, &post_div2))
        panic("System clock of %u kHz cannot be exactly achieved", dvi_clock_khz);
    const uint32_t freq = vco_freq / (post_div1 * post_div2);

    if (timing_mode->bit_clk_khz > 600000) {
        vreg_set_voltage(VREG_VOLTAGE_1_25);
    } else if (timing_mode->bit_clk_khz > 800000) {
        // YOLO mode
        hw_set_bits(&powman_hw->vreg_ctrl, POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_DISABLE_VOLTAGE_LIMIT_BITS);
        vreg_set_voltage(VREG_VOLTAGE_1_40);
        if (timing_mode->bit_clk_khz > 900000) {
            vreg_set_voltage(VREG_VOLTAGE_1_50);
        }
        sleep_ms(1);
    }

    // Set the sys PLL to the requested freq
    pll_init(pll_sys, PLL_COMMON_REFDIV, vco_freq, post_div1, post_div2);

    // CLK HSTX = Requested freq
    clock_configure(clk_hstx,
                    0,
                    CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    freq, freq);
}

void DVHSTX::write_pixel(const Point &p, uint16_t colour)
{
    *point_to_ptr16(p) = colour;
}

void DVHSTX::write_pixel_span(const Point &p, uint l, uint16_t colour)
{
    uint16_t* ptr = point_to_ptr16(p);
    for (uint i = 0; i < l; ++i) ptr[i] = colour;
}

void DVHSTX::write_pixel_span(const Point &p, uint l, uint16_t *data)
{
    uint16_t* ptr = point_to_ptr16(p);
    for (uint i = 0; i < l; ++i) ptr[i] = data[i];
}

void DVHSTX::read_pixel_span(const Point &p, uint l, uint16_t *data)
{
    const uint16_t* ptr = point_to_ptr16(p);
    for (uint i = 0; i < l; ++i) data[i] = ptr[i];
}

void DVHSTX::set_palette(RGB888 new_palette[PALETTE_SIZE])
{
    memcpy(palette, new_palette, PALETTE_SIZE * sizeof(RGB888));
}

void DVHSTX::set_palette_colour(uint8_t entry, RGB888 colour)
{
    palette[entry] = colour;
}

RGB888* DVHSTX::get_palette()
{
    return palette;
}

void DVHSTX::write_palette_pixel(const Point &p, uint8_t colour)
{
    *point_to_ptr_palette(p) = colour;
}

void DVHSTX::write_palette_pixel_span(const Point &p, uint l, uint8_t colour)
{
    uint8_t* ptr = point_to_ptr_palette(p);
    memset(ptr, colour, l);
}

void DVHSTX::write_palette_pixel_span(const Point &p, uint l, uint8_t* data)
{
    uint8_t* ptr = point_to_ptr_palette(p);
    memcpy(ptr, data, l);
}

void DVHSTX::read_palette_pixel_span(const Point &p, uint l, uint8_t *data)
{
    const uint8_t* ptr = point_to_ptr_palette(p);
    memcpy(data, ptr, l);
}

void DVHSTX::write_text(const Point &p, const char* text, TextColour colour, bool immediate)
{
    char* ptr = (char*)point_to_ptr_text(p, immediate);
    int len = std::min((int)(frame_width - p.x), (int)strlen(text));
    memcpy(ptr, text, len);
    if (mode == MODE_TEXT_RGB111) memset(ptr + frame_width * frame_height, (uint8_t)colour, len);
}

void DVHSTX::clear()
{
    memset(frame_buffer_back, 0, frame_width * frame_height * frame_bytes_per_pixel);
}

DVHSTX::DVHSTX()
{
    // Always use the bottom channels
    dma_claim_mask((1 << NUM_CHANS) - 1);
}

bool DVHSTX::init(uint16_t width, uint16_t height, Mode mode_, Pinout pinout)
{
    if (inited) reset();

    ch_num = 0;
    line_num = -1;
    v_scanline = 2;
    flip_next = false;

    display_width = width;
    display_height = height;
    frame_width = width;
    frame_height = height;
    mode = mode_;

    timing_mode = nullptr;
    if (mode == MODE_TEXT_MONO || mode == MODE_TEXT_RGB111) {
        width = 1280;
        height = 720;
        display_width = 91;
        frame_width = 91;
        display_height = 30;
        frame_height = 30;
        h_repeat_shift = 0;
        v_repeat_shift = 0;
        timing_mode = &dvi_timing_1280x720p_rb_50hz;
    }
    else if (width == 320 && height == 180) {
        h_repeat_shift = 2;
        v_repeat_shift = 2;
        timing_mode = &dvi_timing_1280x720p_rb_50hz;
    }
    else if (width == 640 && height == 360) {
        h_repeat_shift = 1;
        v_repeat_shift = 1;
        timing_mode = &dvi_timing_1280x720p_rb_50hz;
    }
    else if (width == 480 && height == 270) {
        h_repeat_shift = 2;
        v_repeat_shift = 2;
        timing_mode = &dvi_timing_1920x1080p_rb2_30hz;
    }
    else
    {
        uint16_t full_width = display_width;
        uint16_t full_height = display_height;
        h_repeat_shift = 0;
        v_repeat_shift = 0;

        if (display_width < 640) {
            h_repeat_shift = 1;
            full_width *= 2;
        }

        if (display_height < 400) {
            v_repeat_shift = 1;
            full_height *= 2;
        }

        if (full_width == 640) {
            if (full_height == 480) timing_mode = &dvi_timing_640x480p_60hz;
        }
        else if (full_width == 720) {
            if (full_height == 480) timing_mode = &dvi_timing_720x480p_60hz;
            else if (full_height == 400) timing_mode = &dvi_timing_720x400p_70hz;
            else if (full_height == 576) timing_mode = &dvi_timing_720x576p_50hz;
        }
        else if (full_width == 800) {
            if (full_height == 600) timing_mode = &dvi_timing_800x600p_60hz;
            else if (full_height == 480) timing_mode = &dvi_timing_800x480p_60hz;
            else if (full_height == 450) timing_mode = &dvi_timing_800x450p_60hz;
        }
        else if (full_width == 960) {
            if (full_height == 540) timing_mode = &dvi_timing_960x540p_60hz;
        }
        else if (full_width == 1024) {
            if (full_height == 768) timing_mode = &dvi_timing_1024x768_rb_60hz;
        }
    }

    if (!timing_mode) {
        dvhstx_debug("Unsupported resolution %dx%d", width, height);
        return false;
    }

    display = this;
    display_palette = get_palette();
    
    dvhstx_debug("Setup clock\n");
    display_setup_clock();

#ifndef MICROPY_BUILD_TYPE
    stdio_init_all();
#endif
    dvhstx_debug("Clock setup done\n");

    v_inactive_total = timing_mode->v_front_porch + timing_mode->v_sync_width + timing_mode->v_back_porch;
    v_total_active_lines = v_inactive_total + timing_mode->v_active_lines;
    v_repeat = 1 << v_repeat_shift;
    h_repeat = 1 << h_repeat_shift;

    memcpy(vblank_line_vsync_off, vblank_line_vsync_off_src, sizeof(vblank_line_vsync_off_src));
    vblank_line_vsync_off[0] |= timing_mode->h_front_porch;
    vblank_line_vsync_off[2] |= timing_mode->h_sync_width;
    vblank_line_vsync_off[4] |= timing_mode->h_back_porch + timing_mode->h_active_pixels;

    memcpy(vblank_line_vsync_on, vblank_line_vsync_on_src, sizeof(vblank_line_vsync_on_src));
    vblank_line_vsync_on[0] |= timing_mode->h_front_porch;
    vblank_line_vsync_on[2] |= timing_mode->h_sync_width;
    vblank_line_vsync_on[4] |= timing_mode->h_back_porch + timing_mode->h_active_pixels;

    memcpy(vactive_line_header, vactive_line_header_src, sizeof(vactive_line_header_src));
    vactive_line_header[0] |= timing_mode->h_front_porch;
    vactive_line_header[2] |= timing_mode->h_sync_width;
    vactive_line_header[4] |= timing_mode->h_back_porch;
    vactive_line_header[6] |= timing_mode->h_active_pixels;

    memcpy(vactive_text_line_header, vactive_text_line_header_src, sizeof(vactive_text_line_header_src));
    vactive_text_line_header[0] |= timing_mode->h_front_porch;
    vactive_text_line_header[2] |= timing_mode->h_sync_width;
    vactive_text_line_header[4] |= timing_mode->h_back_porch;
    vactive_text_line_header[7+6] |= timing_mode->h_active_pixels - 6;

    switch (mode) {
    case MODE_RGB565:
        frame_bytes_per_pixel = 2;
        line_bytes_per_pixel = 2;
        break;
    case MODE_PALETTE:
        frame_bytes_per_pixel = 1;
        line_bytes_per_pixel = 4;
        break;
    case MODE_RGB888:
        frame_bytes_per_pixel = 4;
        line_bytes_per_pixel = 4;
        break;
    case MODE_TEXT_MONO:
        frame_bytes_per_pixel = 1;
        line_bytes_per_pixel = 4;
        break;
    case MODE_TEXT_RGB111:
        frame_bytes_per_pixel = 2;
        line_bytes_per_pixel = 14;
        break;
    default:
        dvhstx_debug("Unsupported mode %d", (int)mode);
        return false;
    }

#ifdef MICROPY_BUILD_TYPE
    if (frame_width * frame_height * frame_bytes_per_pixel > sizeof(frame_buffer_a)) {
        panic("Frame buffer too large");
    }

    frame_buffer_display = frame_buffer_a;
    frame_buffer_back = frame_buffer_b;
#else
    frame_buffer_display = (uint8_t*)malloc(frame_width * frame_height * frame_bytes_per_pixel);
    frame_buffer_back = (uint8_t*)malloc(frame_width * frame_height * frame_bytes_per_pixel);
#endif
    memset(frame_buffer_display, 0, frame_width * frame_height * frame_bytes_per_pixel);
    memset(frame_buffer_back, 0, frame_width * frame_height * frame_bytes_per_pixel);

    memset(palette, 0, PALETTE_SIZE * sizeof(palette[0]));

    frame_buffer_display = frame_buffer_display;
    dvhstx_debug("Frame buffers inited\n");

    const bool is_text_mode = (mode == MODE_TEXT_MONO || mode == MODE_TEXT_RGB111);
    const int frame_pixel_words = (frame_width * h_repeat * line_bytes_per_pixel + 3) >> 2;
    const int frame_line_words = frame_pixel_words + (is_text_mode ? count_of(vactive_text_line_header) : count_of(vactive_line_header));
    const int frame_lines = (v_repeat == 1) ? NUM_CHANS : NUM_FRAME_LINES;
    line_buffers = (uint32_t*)malloc(frame_line_words * 4 * frame_lines);

    for (int i = 0; i < frame_lines; ++i)
    {
        if (is_text_mode) memcpy(&line_buffers[i * frame_line_words], vactive_text_line_header, count_of(vactive_text_line_header) * sizeof(uint32_t));
        else memcpy(&line_buffers[i * frame_line_words], vactive_line_header, count_of(vactive_line_header) * sizeof(uint32_t));
    }

    if (mode == MODE_TEXT_RGB111) {
        // Need to pre-render the font to RAM to be fast enough.
        font_cache = (uint32_t*)malloc(4 * FONT->line_height * GLYPH_COUNT);
        uint32_t* font_cache_ptr = font_cache;
        for (int c = 0; c < GLYPH_COUNT; ++c) {
            for (int y = 0; y < FONT->line_height; ++y) {
                *font_cache_ptr++ = render_char_line(c + FIRST_GLYPH, y);
            }
        }
    }

    // Ensure HSTX FIFO is clear
    reset_block_num(RESET_HSTX);
    sleep_us(10);
    unreset_block_num_wait_blocking(RESET_HSTX);
    sleep_us(10);

    switch (mode) {
    case MODE_RGB565:
        // Configure HSTX's TMDS encoder for RGB565
        hstx_ctrl_hw->expand_tmds =
            4  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            8 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            5  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            3  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            4  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            29 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        // Pixels (TMDS) come in 2 16-bit chunks. Control symbols (RAW) are an
        // entire 32-bit word.
        hstx_ctrl_hw->expand_shift =
            2 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            16 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
        break;

    case MODE_PALETTE:
        // Configure HSTX's TMDS encoder for RGB888
        hstx_ctrl_hw->expand_tmds =
            7  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            16 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            7  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            8  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            7  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            0  << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        // Pixels and control symbols (RAW) are an
        // entire 32-bit word.
        hstx_ctrl_hw->expand_shift =
            1 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
        break;

    case MODE_TEXT_MONO:
        // Configure HSTX's TMDS encoder for 2bpp
        hstx_ctrl_hw->expand_tmds =
            1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            18 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            18  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            18  << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        // Pixels and control symbols (RAW) are an
        // entire 32-bit word.
        hstx_ctrl_hw->expand_shift =
            14 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            30 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
        break;

    case MODE_TEXT_RGB111:
        // Configure HSTX's TMDS encoder for RGB222
        hstx_ctrl_hw->expand_tmds =
            1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            29  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        // Pixels (TMDS) come in 4 8-bit chunks. Control symbols (RAW) are an
        // entire 32-bit word.
        hstx_ctrl_hw->expand_shift =
            4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
        break;

    default:
        dvhstx_debug("Unsupported mode %d", (int)mode);
        return false;
    }

    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS; 

    // HSTX outputs 0 through 7 appear on GPIO 12 through 19.
    constexpr int HSTX_FIRST_PIN = 12;

    // Assign clock pair to two neighbouring pins:
    {
    int bit = pinout.clk_p - HSTX_FIRST_PIN;
    hstx_ctrl_hw->bit[bit    ] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[bit ^ 1] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (uint lane = 0; lane < 3; ++lane) {
        // For each TMDS lane, assign it to the correct GPIO pair based on the
        // desired pinout:
        int bit = pinout.rgb_p[lane] - HSTX_FIRST_PIN;
        // Output even bits during first half of each HSTX cycle, and odd bits
        // during second half. The shifter advances by two bits each cycle.
        uint32_t lane_data_sel_bits =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        // The two halves of each pair get identical data, but one pin is inverted.
        hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit ^ 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, GPIO_FUNC_HSTX);
        gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_4MA);
        if (timing_mode->bit_clk_khz > 900000) {
            gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
        }
    }

    dvhstx_debug("GPIO configured\n");

    // The channels are set up identically, to transfer a whole scanline and
    // then chain to the next channel. Each time a channel finishes, we
    // reconfigure the one that just finished, meanwhile the other channel(s)
    // are already making progress.
    // Using just 2 channels was insufficient to avoid issues with the IRQ.
    dma_channel_config c;
    c = dma_channel_get_default_config(0);
    channel_config_set_chain_to(&c, 1);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        0,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );
    c = dma_channel_get_default_config(1);
    channel_config_set_chain_to(&c, 2);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        1,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );
    for (int i = 2; i < NUM_CHANS; ++i) {
        c = dma_channel_get_default_config(i);
        channel_config_set_chain_to(&c, (i+1) % NUM_CHANS);
        channel_config_set_dreq(&c, DREQ_HSTX);
        dma_channel_configure(
            i,
            &c,
            &hstx_fifo_hw->fifo,
            vblank_line_vsync_off,
            count_of(vblank_line_vsync_off),
            false
        );
    }

    dvhstx_debug("DMA channels claimed\n");

    dma_hw->intr = (1 << NUM_CHANS) - 1;
    dma_hw->ints2 = (1 << NUM_CHANS) - 1;
    dma_hw->inte2 = (1 << NUM_CHANS) - 1;
    if (is_text_mode) irq_set_exclusive_handler(DMA_IRQ_2, dma_irq_handler_text);
    else irq_set_exclusive_handler(DMA_IRQ_2, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_2, true);

    dma_channel_start(0);

    dvhstx_debug("DVHSTX started\n");

    for (int i = 0; i < frame_height; ++i) {
        memset(&frame_buffer_display[i * frame_width * frame_bytes_per_pixel], i, frame_width * frame_bytes_per_pixel);
    }

    dvhstx_debug("Frame buffer filled\n");

    inited = true;
    return true;
}

void DVHSTX::reset() {
    if (!inited) return;
    inited = false;

    hstx_ctrl_hw->csr = 0;

    irq_set_enabled(DMA_IRQ_2, false);
    irq_remove_handler(DMA_IRQ_2, irq_get_exclusive_handler(DMA_IRQ_2));

    for (int i = 0; i < NUM_CHANS; ++i)
        dma_channel_abort(i);

    if (font_cache) {
        free(font_cache);
        font_cache = nullptr;
    }
    free(line_buffers);

#ifndef MICROPY_BUILD_TYPE
    free(frame_buffer_display);
    free(frame_buffer_back);
#endif
}

void DVHSTX::flip_blocking() {
    wait_for_vsync();
    flip_now();
}

void DVHSTX::flip_now() {
    std::swap(frame_buffer_display, frame_buffer_back);
}

void DVHSTX::wait_for_vsync() {
    while (v_scanline >= timing_mode->v_front_porch) __wfe();
}

void DVHSTX::flip_async() {
    flip_next = true;
}

void DVHSTX::wait_for_flip() {
    while (flip_next) __wfe();
}