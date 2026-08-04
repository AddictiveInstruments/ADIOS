# g030k6_minimal - CubeMX-style minimal smoke test, no MIOS32 build system
# involved at all. Just: startup file (vector table + reset handler) + a raw
# main.c driving PC6 high via CMSIS register access + a linker script with
# the vector table at the fixed physical boot address 0x08000000.

MIOS32_PATH ?= ../../..
TOOLCHAIN   ?= /c/ST/STM32CubeIDE_1.16.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130/tools/bin

CC      = $(TOOLCHAIN)/arm-none-eabi-gcc
OBJCOPY = $(TOOLCHAIN)/arm-none-eabi-objcopy
SIZE    = $(TOOLCHAIN)/arm-none-eabi-size

CMSIS_INC = $(MIOS32_PATH)/drivers/STM32G0xx/LL_HAL/CMSIS/Include
DEV_INC   = $(MIOS32_PATH)/drivers/STM32G0xx/LL_HAL/CMSIS/Device/ST/STM32G0xx/Include

CFLAGS = -mcpu=cortex-m0plus -mthumb -O0 -g -ffunction-sections -fdata-sections \
         -DMIOS32_PROCESSOR_STM32G030K6 -DGCC_ARMCM0 \
         -I $(CMSIS_INC) -I $(DEV_INC)

LDFLAGS = -mcpu=cortex-m0plus -mthumb -nostartfiles -Wl,--gc-sections \
          -T ld/STM32G030K6_noboot.ld -Wl,-Map=project.map

all: project.elf project.bin
	$(SIZE) project.elf

project.elf: startup_stm32g030.o main.o
	$(CC) $(LDFLAGS) startup_stm32g030.o main.o -o project.elf

startup_stm32g030.o: $(MIOS32_PATH)/programming_models/traditional/startup_stm32g030.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

project.bin: project.elf
	$(OBJCOPY) -O binary project.elf project.bin

clean:
	rm -f *.o *.elf *.bin *.map

.PHONY: all clean
