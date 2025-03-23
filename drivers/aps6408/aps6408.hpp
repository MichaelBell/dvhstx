#pragma once

#include <stdint.h>
#include "hardware/pio.h"
#include "hardware/dma.h"

namespace pimoroni {
    class APS6408 {
        public:
            static constexpr int RAM_SIZE = 8 * 1024 * 1024;
            static constexpr int PAGE_SIZE = 1024;

            // If two APS6408s are instantiated, they must use the same PIO and different IRQs.
            // DMA IRQ must be 0 or 1, an exclusive handler is installed.
            APS6408(uint pin_sck = 12, uint pin_d0 = 4, uint pin_dqs = 14, uint pin_rst = 15, uint dma_irq = 0, PIO pio = pio0);

            void init();

            // Start a write, this completes asynchronously, this function blocks if another 
            // transfer is already in progress
            // Writes must be word aligned.
            void write(uint32_t addr, const uint32_t* data, uint32_t len_in_words);

            // Fastest write function, must not cross 1kB boundaries
            void write_raw(uint32_t addr, const uint32_t* data, uint32_t len_in_words);

            // Write and block until completion
            void write_blocking(uint32_t addr, const uint32_t* data, uint32_t len_in_words) {
                write(addr, data, len_in_words);
                dma_channel_wait_for_finish_blocking(write_dma_channel);
            }

            // Fill memory with a single word.  Blocks.
            // Writes must be word aligned.
            void write_repeat(uint32_t addr, const uint32_t data, uint32_t len_in_words);

            // Start a read, this completes asynchronously, this function only blocks if another 
            // transfer is already in progress
            // Reads must be word aligned.
            void read(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words);

            // Fastest read function, must not cross 1kB boundaries
            void read_raw(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words);

            // Read and block until completion
            void read_blocking(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words) {
                read(addr, read_buf, len_in_words);
                dma_channel_wait_for_finish_blocking(read_dma_channel);
            }

            void wait_for_read_blocking() {
                dma_channel_wait_for_finish_blocking(read_dma_channel);
            }

        private:
            void setup_dma_config();

            uint pin_sck;  // SCK, CSn must be next pin after SCK
            uint pin_d0;   // D0-D7 must be consecutive
            uint dma_irq;

            PIO pio;
            uint16_t pio_read_sm;
            uint16_t pio_command_sm;

            uint write_dma_channel;
            uint read_dma_channel;
    };
}
