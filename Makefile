# Name: tftp.c
# Author: Denis PEROTTO, from work published by Arduino
# Copyright: Arduino
# License: GPL http://www.gnu.org/licenses/gpl-2.0.html
# Project: eboot
# Function: tftp implementation and flasher
# Version: 0.1 tftp / flashing functional

# Important make targets:
# install			: flash bootloader using AVR ISP MkII
# disasm			: output disassembly of bootloader
# readback		: read back flash contents to read.hex


DEVICE     = atmega328p
CLOCK      = 16000000
PROGRAMMER = -c usbtiny -P usb
OBJECTS    = main.o net.o tftp.o validate.o debug.o
APPOBJECTS = app.o

# 2Kword boot block
FUSES      = -U efuse:w:0x05:m -U hfuse:w:0xd8:m -U lfuse:w:0xff:m
#FUSES      = -U efuse:w:0x05:m -U hfuse:w:0xd9:m -U lfuse:w:0xff:m
LDFLAGS = -Ttext=0x7000

# 1Kword boot block
#FUSES      = -U efuse:w:0x05:m -U hfuse:w:0xda:m -U lfuse:w:0xff:m
#LDFLAGS = -Ttext=0x7800

# 512 word boot block
#FUSES      = -U efuse:w:0x05:m -U hfuse:w:0xdc:m -U lfuse:w:0xff:m
#LDFLAGS = -Ttext=0x7C00

# 256 word boot block
#FUSES      = -U efuse:w:0x05:m -U hfuse:w:0xde:m -U lfuse:w:0xff:m
#LDFLAGS = -Ttext=0x7E00

# Extended Fuse Byte:
# 0x05 = 0 0 0 0 0 1 0 1
#        \---+---/ \-+-/
#            |       +------ BODLEVEL Brownout detect at 2.5 to 2.9V
#            +-------------- Unused
#
# High Fuse Byte:
# 0xD8 = 1 1 0 1 1 0 0 0 <-- BOOTRST  Reset to bootloader
#        ^ ^ ^ ^ ^ \+/
#        | | | | |  +------- BOOTSZ   2Kword bootloader, 0x3800-0x3FFF, reset at 0x3800
#        | | | | +---------- EESAVE   EEPROM erased in chip erase
#        | | | +------------ WDTON    Watchdog timer off
#        | | +-------------- SPIEN    SPI programming enabled
#        | +---------------- DWEN     DebugWIRE disabled
#        +------------------ RSTDISBL External Reset enabled
#
# Low Fuse Byte:
# 0xFF = 1 1 1 1 1 1 1 1
#        | | \+/ \--+--/
#        | |  |     +------- CKSEL    Low power crystal, 16Kck delay
#        | |  +------------- SUT      65ms delay
#        | +---------------- CKOUT    No clock out
#        +------------------ CKDIV8   No divide by 8 prescaler

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)

# Compiler options to shrink bootloader size
#

CCOPT = -Os
CCOPT += -mno-interrupts #Interrupts will not be supported in code
CCOPT += -mshort-calls
CCOPT += -fno-inline-small-functions
CCOPT += -fno-split-wide-types
CCOPT += -Wl,--relax
# do not use cause great instability??
#CCOPT += -nostartfiles

# These optimisations have no effect, so turned off
#
#CCOPT += -mtiny-stack
#CCOPT += -ffreestanding
#CCOPT += -fpack-struct
#CCOPT += -fno-jump-tables

COMPILE = avr-gcc -Wall $(CCOPT) -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump

# symbolic targets:
all:	main.hex  

%.o: %.c
	$(COMPILE) -c $< -o $@

%.o: %.S
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

%.s: %.c
	$(COMPILE) -S $< -o $@

%.bin: %.elf
	rm -f $@
	$(OBJCOPY) -j .text -j .data -O binary $< $@

%.hex: %.elf
	rm -f $@
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

# Programming targets - which are set up for a TinyUSB programmer
flash:	main.hex
	$(AVRDUDE) -U flash:w:main.hex:i
quickflash:	main.hex
	$(AVRDUDE) -V -U flash:w:main.hex:i
fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse
quickinstall:quickflash fuse
clean:
	rm -f *.hex *.elf *.bin *.o

# Bootloader
#
main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS) $(LDFLAGS)

# Targets for code debugging and analysis:
disasm:	main.elf
	$(OBJDUMP) -d main.elf

map:	main.elf
	$(OBJDUMP) -h main.elf

cpp:
	$(COMPILE) -E main.c

readback:
	$(AVRDUDE) -U flash:r:read.hex:i



