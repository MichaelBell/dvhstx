// Board and hardware specific configuration
#define MICROPY_HW_BOARD_NAME                   "Mini DVHSTX"
#define MICROPY_HW_FLASH_STORAGE_BYTES          (PICO_FLASH_SIZE_BYTES - 1024 * 1024)

extern void display_setup_clock_preinit();
#define MICROPY_BOARD_STARTUP() display_setup_clock_preinit()
