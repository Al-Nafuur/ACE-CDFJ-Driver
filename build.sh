#!/bin/bash
SDK="United-Carts-of-Atari"
INC="$INC -I$SDK/source/STM32firmware/PlusCart/Inc"
INC="$INC -I$SDK/source/STM32firmware/PlusCart/Drivers/STM32F4xx_HAL_Driver/Inc"
INC="$INC -I$SDK/source/STM32firmware/PlusCart/Drivers/CMSIS/Device/ST/STM32F4xx/Include/"
INC="$INC -I$SDK/source/STM32firmware/PlusCart/Drivers/CMSIS/Include/"

WARNINGS="-Wall -Wswitch-default -Wswitch-enum -Wconversion"
FLGS="-mcpu=cortex-m4 -mthumb -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F407xx -DVERSION="2.3.17" -DUNOCART=1 -DPLUSCART=2 -DHARDWARE_TYPE=PLUSCART -DMENU_TYPE=PLUSCART -DUSE_WIFI=1 -DUSE_SD_CARD=0 -ffunction-sections -fdata-sections -fPIC -MMD -MP -mfpu=fpv4-sp-d16 -mfloat-abi=hard" 

DWARF="-g3 -gdwarf-4 -gstrict-dwarf"
OPT="-O3"

LNKFLGS="-mcpu=cortex-m4 -mthumb -static -specs=nano.specs -fpic -mfpu=fpv4-sp-d16 -mfloat-abi=hard -lc -lm -nostartfiles" 

if [ ! -d bin ]; then
	mkdir bin
fi

arm-none-eabi-gcc $FLGS $INC $OPT $DWARF -c src/driver.c -o bin/driver.o

# first compile contains DWARF data
arm-none-eabi-gcc $LNKFLGS -Tsrc/driver.ld -Wl,-Map=bin/debugging.map bin/*.o -o bin/debugging.elf
arm-none-eabi-objdump -S bin/debugging.elf > bin/debugging.obj

# second compile without DWARF data (for some reason objcopy does not strip 
# out our DWARF data)

arm-none-eabi-gcc -s $LNKFLGS -Tsrc/driver.ld -Wl,-Map=bin/driver.map bin/*.o -o bin/driver.elf
arm-none-eabi-objcopy -O binary bin/driver.elf bin/driver.bin

dasm src/ace.asm -Ibin -d -lbin/driver.lst -sbin/driver.sym -f3 -v5 -obin/driver.ace
