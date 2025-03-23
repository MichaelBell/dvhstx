# cmake file for Raspberry Pi Pico2
set(PICO_BOARD "pico2")

# Board specific version of the frozen manifest
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# If USER_C_MODULES or MicroPython customisations use malloc then
# there needs to be some RAM reserved for the C heap
set(MICROPY_C_HEAP_SIZE 32768)

set(PIMORONI_UF2_MANIFEST ${MICROPY_BOARD_DIR}/manifest.txt)
set(PIMORONI_UF2_DIR ${CMAKE_CURRENT_LIST_DIR}/../../micropython/examples)
include(${CMAKE_CURRENT_LIST_DIR}/../common.cmake)
