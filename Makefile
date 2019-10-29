# Example of compiling a simple bare-metal logic analyzer
# for an ARM Cortex-M microcontroller, e.g.
# Infineon XMC4500 Relax Lite Kit

SHELL=/bin/bash

# set up paths

# Requires the GNU Arm Embedded Toolchain installed, for example on
# Ubuntu Linux: gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch

# If $(TOOLDIR) does not exist then default to .
TOOLDIR:=$(wildcard $(TOOLDIR))
# Select which pre-compiled ARM toolchain binaries to use
ARCH=$(shell uname -s)
ifndef TOOLDIR
TOOLDIR=.
endif
XMCLIB:=$(TOOLDIR)/xmclib

# configure the C compiler

CC         = arm-none-eabi-gcc
OBJDUMP    = arm-none-eabi-objdump
OBJCOPY    = arm-none-eabi-objcopy
ARCH_FLAGS = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
STARTUP    = $(XMCLIB)/CMSIS/Infineon/XMC4500_series/Source/GCC/startup_XMC4500.S $(XMCLIB)/CMSIS/Infineon/XMC4500_series/Source/system_XMC4500.c $(XMCLIB)/XMCLib/src/xmc4_gpio.c $(XMCLIB)/XMCLib/src/xmc_gpio.c

STARTUP_DEFS=-D__STARTUP_CLEAR_BSS -D__START=main
CPPFLAGS   = -I$(XMCLIB)/XMCLib/inc -I$(XMCLIB)/CMSIS/Include -I$(XMCLIB)/CMSIS/Infineon/XMC4500_series/Include -DXMC4500_F100x1024
CFLAGS     = $(CPPFLAGS) $(ARCH_FLAGS) $(STARTUP_DEFS) -g -flto -O
LDSCRIPTS  =-L. -L$(XMCLIB)/CMSIS/Infineon/XMC4500_series/Source/GCC/ -T XMC4500x1024.ld
LFLAGS    = --specs=nosys.specs $(LDSCRIPTS) -Wl,--gc-sections -Wl,-Map=$*.map

all: sniffer.elf

%.elf: %.c $(STARTUP)
	$(CC) $^ $(CFLAGS) $(LFLAGS) -o $@

%.asm: %.elf
	$(OBJDUMP) -S $< > $@

%.bin: %.elf
	$(OBJCOPY) $< -O binary $@

# start debugger link server
# (the default speed 'adapter_khz 1000' seems too fast for some
#  XMC4500 Relax Lite boards, so reduce if you get JAYLINK_ERR)
KHZ=800
ocd:
	openocd -f board/xmc4500-relax.cfg -c 'adapter_khz $(KHZ)'

#GDB=arm-none-eabi-gdb-py
#GDB=arm-none-eabi-gdb
GDB=gdb-multiarch  # this usues python3 on Ubuntu 18.04

# upload application into FLASH using debugger, then run it there
sniff: sniffer.elf
	./ocdcmd 'ocd_reset halt'
	which $(GDB)
	$(GDB) -batch -ex 'source sniffer.py' sniffer.elf

clean:
	rm -f *.elf *.asm *.bin *.map

# Infineon's XMC Lib (also part of DAVE)
$(XMCLIB): $(TOOLDIR)/XMC_Peripheral_Library_v2.1.24.zip
	unzip $< $(basename $(notdir $<))'/*' -d $(TOOLDIR)
	ln -s $(basename $(notdir $<)) $@

$(TOOLDIR)/XMC_Peripheral_Library_%.zip:
	cd $(TOOLDIR) && curl -O http://dave.infineon.com/Libraries/XMCLib/$(notdir $@)
