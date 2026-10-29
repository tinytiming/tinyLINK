cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(tinylink C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(tinylink
  src/main.c
  src/usb_descriptors.c
  src/usbio.c
  src/console.c
  src/led.c
  src/button.c
  src/settings.c
  src/power.c
  src/cdc_uart.c
  src/mode_dap.c
  src/mode_uart.c
  src/mode_updi.c
  src/mode_wch.c
  src/mode_la.c
  src/mode_serprog.c
  src/mode_avrisp.c
  third_party/free-dap/dap.c
)

pico_generate_pio_header(tinylink ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)
pico_generate_pio_header(tinylink ${CMAKE_CURRENT_LIST_DIR}/src/od_uart.pio)
pico_generate_pio_header(tinylink ${CMAKE_CURRENT_LIST_DIR}/src/swio.pio)
pico_generate_pio_header(tinylink ${CMAKE_CURRENT_LIST_DIR}/src/la.pio)

target_include_directories(tinylink PRIVATE src third_party/free-dap)
target_compile_options(tinylink PRIVATE -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(tinylink
  pico_stdlib
  pico_unique_id
  hardware_adc
  hardware_dma
  hardware_flash
  hardware_pio
  hardware_spi
  hardware_uart
  tinyusb_device
)

pico_add_extra_outputs(tinylink)
