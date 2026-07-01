cmake_minimum_required(VERSION 3.20)

# ====================================================================
# FSGlove v2 — STM32H7 采集端固件 CMake 工程
#
# 目标 MCU : STM32H750IBK6 (UFBGA176, 480MHz, 内部 Flash 仅 128KB)
#            开发可先用 Nucleo-H743ZI2 验证（引脚兼容，仅 Flash/型号宏不同）
# 器件宏   : STM32H750xx, 480 MHz
# ⚠ H750 内部 Flash 只有 128KB：本采集固件可放下；超出需走外部 QSPI/OctoSPI XIP。
#
# 首次 configure 会用 FetchContent 从 ST 官方仓库拉取：
#   - cmsis_core            (CMSIS-Core, core_cm7.h 等)
#   - cmsis_device_h7       (设备头 + startup + system 文件)
#   - stm32h7xx_hal_driver  (HAL 驱动源码)
# 需要网络。也可改用本地已装的 STM32CubeH7（见 README）。
# ====================================================================

# 工具链文件需在 project() 之前生效（用 -DCMAKE_TOOLCHAIN_FILE 或 preset）
project(fsglove_stm32h7 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- 依赖版本（均为已确认存在的 tag；如需更新去对应仓库 Tags 页改这里） ----
# 用 ST 精简组件仓库（体积小、tag 与 CubeH7 对齐），CMSIS-Core 用 ST cmsis_core。
set(CMSIS_CORE_TAG   "v5.9.0"  CACHE STRING "STMicroelectronics/cmsis_core tag")
set(CMSIS_DEVICE_TAG "v1.10.7" CACHE STRING "STMicroelectronics/cmsis_device_h7 tag")
set(HAL_DRIVER_TAG   "v1.11.6" CACHE STRING "STMicroelectronics/stm32h7xx_hal_driver tag")

include(FetchContent)
FetchContent_Declare(cmsis_core
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_core.git
    GIT_TAG        ${CMSIS_CORE_TAG}
    GIT_SHALLOW    TRUE)
FetchContent_Declare(cmsis_device_h7
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_h7.git
    GIT_TAG        ${CMSIS_DEVICE_TAG}
    GIT_SHALLOW    TRUE)
FetchContent_Declare(stm32h7_hal
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git
    GIT_TAG        ${HAL_DRIVER_TAG}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(cmsis_core cmsis_device_h7 stm32h7_hal)

# ---- 厂商源文件 ----
set(STARTUP_FILE ${cmsis_device_h7_SOURCE_DIR}/Source/Templates/gcc/startup_stm32h750xx.s)
set(SYSTEM_FILE  ${cmsis_device_h7_SOURCE_DIR}/Source/Templates/system_stm32h7xx.c)

# HAL 源：全量编译，未启用的模块由 stm32h7xx_hal_conf.h 的宏裁掉（文件级 #ifdef 保护）
file(GLOB HAL_SOURCES ${stm32h7_hal_SOURCE_DIR}/Src/*.c)
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\\.c$")

# ---- 工程源文件 ----
set(PROJECT_SOURCES
    Core/Src/main.c
    Core/Src/stm32h7xx_it.c
    Core/Src/stm32h7xx_hal_msp.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    App/acquisition.c
    App/imu_node.c
    App/spi_bus.c
    App/cs_decoder.c
    App/esp_link.c
    Drivers/imu/lsm6dsox.c
    Drivers/imu/lis3mdl.c
)

add_executable(${PROJECT_NAME}.elf
    ${PROJECT_SOURCES}
    ${HAL_SOURCES}        # ST HAL 驱动源（未启用模块由 #ifdef 裁掉）
    ${SYSTEM_FILE}
    ${STARTUP_FILE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    App
    Drivers/imu
    ${CMAKE_CURRENT_SOURCE_DIR}/../common         # fsglove_protocol.h
    ${cmsis_core_SOURCE_DIR}/Include               # ST cmsis_core: core_cm7.h 等
    ${cmsis_device_h7_SOURCE_DIR}/Include
    ${stm32h7_hal_SOURCE_DIR}/Inc
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    STM32H750xx
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -g>
)

# ---- 链接 ----
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32H750IBKx_FLASH.ld)
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments   # 抑制新版 binutils 对合并 RWX 段的无害告警
    -Wl,--print-memory-usage
    -Wl,-Map=${PROJECT_NAME}.map
    -lc -lm
    # 不链 -lnosys：syscalls.c / sysmem.c 自带桩与堆界管理，避免 _sbrk 重复定义
)
set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# ---- 产物：.hex / .bin + size ----
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating .hex / .bin and size")
