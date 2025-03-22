#include <algorithm>
#include "aps6408.hpp"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include <cstdio>

#ifndef NO_QSTR
#include "aps6408.pio.h"
#endif

#ifndef MICROPY_BUILD_TYPE
#define mp_printf(_, ...) printf(__VA_ARGS__);
#else
extern "C" {
#include "py/runtime.h"
}
#endif

namespace {
    void aps6408_read_program_init(PIO pio, uint sm, uint offset, uint d0) {
        pio_sm_config c = aps6408_read_program_get_default_config(offset);
        sm_config_set_in_pins(&c, d0);
        sm_config_set_in_shift(&c, true, true, 32);
        sm_config_set_out_pins(&c, d0, 8);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

        pio_sm_init(pio, sm, offset, &c);
        pio_sm_set_enabled(pio, sm, true);
    }
    void aps6408_command_program_init(PIO pio, uint sm, uint offset, uint sck, uint d0) {
        pio_sm_config c = aps6408_command_program_get_default_config(offset);
        sm_config_set_in_pins(&c, d0);
        sm_config_set_in_shift(&c, true, true, 32);
        sm_config_set_out_pins(&c, d0, 8);
        sm_config_set_out_shift(&c, true, true, 32);
        sm_config_set_sideset_pins(&c, sck);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

        pio_gpio_init(pio, sck);
        pio_gpio_init(pio, sck + 1);

        for (uint i = 0; i < 8; ++i)
            pio_gpio_init(pio, d0 + i);

        pio_sm_set_consecutive_pindirs(pio, sm, sck, 2, true);
        pio_sm_set_consecutive_pindirs(pio, sm, d0, 8, false);

        pio_sm_init(pio, sm, offset, &c);
        pio_sm_set_enabled(pio, sm, true);
    }

    PIO aps6408_pio;
    uint aps6408_read_sm_0;
    uint aps6408_read_sm_1;
    int pio_read_offset = -1;
    int pio_command_offset = -1;
    uint pio_command_write;
    uint pio_command_read;

    void __no_inline_not_in_flash_func(reset_read_sm_0)()
    {
        pio_sm_set_enabled(aps6408_pio, aps6408_read_sm_0, false);
        pio_sm_clear_fifos(aps6408_pio, aps6408_read_sm_0);
        pio_sm_restart(aps6408_pio, aps6408_read_sm_0);
        pio_sm_exec(aps6408_pio, aps6408_read_sm_0, pio_encode_jmp(pio_read_offset));
        pio_sm_set_enabled(aps6408_pio, aps6408_read_sm_0, true);

        // Acknowledge channel - assume this is an exclusive handler 
        // so just acknowledge whatever is enabled
        dma_hw->ints0 = dma_hw->inte0;
    }

    void __no_inline_not_in_flash_func(reset_read_sm_1)()
    {
        pio_sm_set_enabled(aps6408_pio, aps6408_read_sm_1, false);
        pio_sm_clear_fifos(aps6408_pio, aps6408_read_sm_1);
        pio_sm_restart(aps6408_pio, aps6408_read_sm_1);
        pio_sm_exec(aps6408_pio, aps6408_read_sm_1, pio_encode_jmp(pio_read_offset));
        pio_sm_set_enabled(aps6408_pio, aps6408_read_sm_1, true);

        // Acknowledge channel - assume this is an exclusive handler 
        // so just acknowledge whatever is enabled
        dma_hw->ints1 = dma_hw->inte1;
    }
}

namespace pimoroni {
    APS6408::APS6408(uint pin_sck, uint pin_d0, uint pin_dqs, uint pin_rst, uint dma_irq, PIO pio)
                : pin_sck(pin_sck)
                , pin_d0(pin_d0)
                , dma_irq(dma_irq)
                , pio(pio)
    {
        // Initialize data pins
        for (int i = 0; i < 8; ++i) {
            gpio_init(pin_d0 + i);
            gpio_set_pulls(pin_d0 + i, false, true);
        }

        gpio_set_drive_strength(pin_sck, GPIO_DRIVE_STRENGTH_8MA);

        gpio_init(pin_sck + 1);
        gpio_pull_up(pin_sck + 1);

        // Initialize dqs
        gpio_init(pin_dqs);
        gpio_set_dir(pin_dqs, false);

        // Reset
        sleep_us(150);
        gpio_init(pin_rst);
        gpio_put(pin_rst, 0);
        gpio_set_dir(pin_rst, true);
        sleep_us(10);
        gpio_put(pin_rst, 1);
        sleep_us(10);

        pio_command_sm = pio_claim_unused_sm(pio, true);
        pio_read_sm = pio_claim_unused_sm(pio, true);
        aps6408_read_sm_0 = pio_read_sm;

        if (pio_command_offset == -1) {
            aps6408_pio = pio;
            pio_command_offset = pio_add_program(pio, &aps6408_command_program);
            pio_read_offset = pio_add_program(pio, &aps6408_read_program);

            pio_command_write = pio_command_offset + aps6408_command_offset_do_write;
            pio_command_read = pio_command_offset + aps6408_command_offset_do_read;
        }

        // Claim DMA channels
        write_dma_channel = dma_claim_unused_channel(true);
        read_dma_channel = dma_claim_unused_channel(true);
        setup_dma_config();
    }

    void APS6408::init() {
        aps6408_command_program_init(pio, pio_command_sm, pio_command_offset, pin_sck, pin_d0);
        aps6408_read_program_init(pio, pio_read_sm, pio_read_offset, pin_d0);

        sleep_us(200);
        // Set read latency to fixed and default 10 cycles (max 133MHz)
        pio_sm_put_blocking(pio, pio_command_sm, 0xc0ff0000u);
        pio_sm_put_blocking(pio, pio_command_sm, 0x00000000u);
        pio_sm_put_blocking(pio, pio_command_sm, pio_command_offset + aps6408_command_offset_do_reg_write);
        pio_sm_put_blocking(pio, pio_command_sm, 0x28u | (pio_command_offset << 16));

        // Set write latency to 3 cycles (max 66MHz)
        pio_sm_put_blocking(pio, pio_command_sm, 0xc0ff0000u);
        pio_sm_put_blocking(pio, pio_command_sm, 0x04000000u);
        pio_sm_put_blocking(pio, pio_command_sm, pio_command_offset + aps6408_command_offset_do_reg_write);
        pio_sm_put_blocking(pio, pio_command_sm, 0x00u | (pio_command_offset << 16));
    }

    void APS6408::setup_dma_config() {
        irq_set_exclusive_handler(DMA_IRQ_0 + dma_irq, (dma_irq & 1) ? reset_read_sm_1 : reset_read_sm_0);
        switch (dma_irq) {
            case 0:
                dma_hw->ints0 = dma_hw->ints0;
                dma_hw->inte0 = 1u << read_dma_channel;
                break;
            case 1:
                dma_hw->ints1 = dma_hw->ints1;
                dma_hw->inte1 = 1u << read_dma_channel;
                break;
            default:
                panic("Invalid DMA irq %d", dma_irq);
        }
        // Must be able to pre-empt if read is called from an interrupt handler.
        irq_set_priority(DMA_IRQ_0 + dma_irq, 0x40);
        irq_set_enabled(DMA_IRQ_0 + dma_irq, true);

        dma_channel_config c = dma_channel_get_default_config(read_dma_channel);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, pio_get_dreq(pio, pio_read_sm, false));
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        
        dma_channel_configure(
            read_dma_channel, &c,
            nullptr,
            &pio->rxf[pio_read_sm],
            0,
            false
        );

        c = dma_channel_get_default_config(write_dma_channel);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio, pio_command_sm, true));
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

        dma_channel_configure(
            write_dma_channel, &c,
            &pio->txf[pio_command_sm],
            nullptr,
            0,
            false
        );
    }

    void APS6408::write(uint32_t addr, const uint32_t* data, uint32_t len_in_words) {
        while (len_in_words) {
            uint32_t len = std::min((0x200 - (addr & 0x1FF)) >> 2, len_in_words);

            write_raw(addr, data, len);
            len_in_words -= len;
            addr += len << 2;
            data += len;
        }
    }

    void APS6408::write_repeat(uint32_t addr, const uint32_t data, uint32_t len_in_words) {
        dma_channel_wait_for_finish_blocking(write_dma_channel);
        hw_clear_bits(&dma_hw->ch[write_dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);

        while (len_in_words) {
            uint32_t len = std::min((0x400 - (addr & 0x3FF)) >> 2, len_in_words);

            write_raw(addr, &data, len);
            len_in_words -= len;
            addr += len << 2;
        }

        dma_channel_wait_for_finish_blocking(write_dma_channel);
        hw_set_bits(&dma_hw->ch[write_dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);
    }

    void __no_inline_not_in_flash_func(APS6408::read)(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words) {
        while (len_in_words) {
            uint32_t len = std::min((0x400 - (addr & 0x3FF)) >> 2, len_in_words);

            read_raw(addr, read_buf, len);
            dma_channel_wait_for_finish_blocking(read_dma_channel);
            len_in_words -= len;
            addr += len << 2;
            read_buf += len;
        }
    }

    void __no_inline_not_in_flash_func(APS6408::write_raw)(uint32_t addr, const uint32_t* data, uint32_t len_in_words) {
        dma_channel_wait_for_finish_blocking(write_dma_channel);

        pio_sm_put_blocking(pio, pio_command_sm, ((len_in_words << 1) - 1) | 0xa0ff0000);
        pio_sm_put_blocking(pio, pio_command_sm, __bswap32(addr));
        pio_sm_put_blocking(pio, pio_command_sm, pio_command_write);

        dma_channel_transfer_from_buffer_now(write_dma_channel, data, len_in_words);
    }

    void __no_inline_not_in_flash_func(APS6408::read_raw)(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words) {
        dma_channel_wait_for_finish_blocking(write_dma_channel);

        pio_sm_put_blocking(pio, pio_command_sm, ((len_in_words << 1) + 8) | 0x20ff0000);
        pio_sm_put_blocking(pio, pio_command_sm, __bswap32(addr));
        pio_sm_put_blocking(pio, pio_command_sm, pio_command_read);

        dma_channel_wait_for_finish_blocking(read_dma_channel);
        
        dma_channel_transfer_to_buffer_now(read_dma_channel, read_buf, len_in_words);
    }
}