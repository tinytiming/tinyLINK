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

# version.h comes from `git describe`, regenerated on every build (rewritten
# only when the version changes) so a new tag needs no re-configure.
find_package(Git QUIET)
set(TINYLINK_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(TINYLINK_VERSION_ARGS -DGIT=${GIT_EXECUTABLE} -DSRC=${CMAKE_CURRENT_LIST_DIR}
    -DOUT=${TINYLINK_GENERATED}/version.h -P ${CMAKE_CURRENT_LIST_DIR}/cmake/version.cmake)
execute_process(COMMAND ${CMAKE_COMMAND} ${TINYLINK_VERSION_ARGS})
add_custom_target(tinylink_version
  COMMAND ${CMAKE_COMMAND} ${TINYLINK_VERSION_ARGS}
  BYPRODUCTS ${TINYLINK_GENERATED}/version.h
  COMMENT "tinyLINK version from git describe")
add_dependencies(tinylink tinylink_version)

target_include_directories(tinylink PRIVATE ${TINYLINK_GENERATED} src third_party/free-dap)

option(LED_ORDER_RGB "RP2040-Zero LED takes R,G,B order instead of G,R,B" OFF)
if(LED_ORDER_RGB)
  target_compile_definitions(tinylink PRIVATE LED_ORDER_RGB=1)
endif()

# -DPICO_BOARD=pico: a plain Raspberry Pi Pico, e.g. on the picoprobe PCB.
if(PICO_BOARD STREQUAL "pico")
  target_compile_definitions(tinylink PRIVATE TINYLINK_BOARD_PICO=1)
endif()
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
