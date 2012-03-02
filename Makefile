# Name: Makefile
# Author: Jens W. Johannsen
# Copyright: © JWJ 2012

# This is a prototype Makefile. Modify it according to your needs.
# You should at least check the settings for
# DEVICE ....... The AVR device you compile for
# CLOCK ........ Target AVR clock rate in Hertz
# OBJECTS ...... The object files created from your source files. This list is
#                usually the same as the list of source files with suffix ".o".
# PROGRAMMER ... Options to avrdude which define the hardware you use for
#                uploading to the AVR and the interface where this hardware
#                is connected.
# FUSES ........ Parameters for avrdude to flash the fuses appropriately.

DEVICE     = atmega328p
CLOCK      = 8000000
PROGRAMMER = -c avrispmkII -P usb
OBJECTS    = main.o DS1620.o
FUSES      = -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m	# Fuses for ATmega168/ATtiny25: Int 8/8 MHz RC osc.(slow startup); no BOD; SPI enabled


# AVR fuse calculator
# http://www.engbedded.com/fusecalc/

# Tune the lines below only if you know what you are doing:

.SUFFIXES: .c .o .h .S .s
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
#COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -W -Os -mcall-prologues -fshort-enums  -Wl,-u,vfprintf -lprintf_min -DF_CPU=$(CLOCK)
COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -W -Os -mcall-prologues -fshort-enums  -Wl,-u,vfprintf -lm -DF_CPU=$(CLOCK)

##################
# Symbolic targets
##################

all: clean main.hex

build: clean main.hex

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

install: flash fuse

clean:
	rm -f main.hex main.elf *.o


##############
# Suffix rules
##############

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@


##############
# File targets
##############

#####################
# tuxgraphics targets

tuxgraphics: ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o

websrv_help_functions.o: tuxgraphics/websrv_help_functions.c tuxgraphics/websrv_help_functions.h ip_config.h 
	$(COMPILE) -I. -Os -c tuxgraphics/websrv_help_functions.c
	
dnslkup.o: tuxgraphics/dnslkup.c  tuxgraphics/dnslkup.h
	$(COMPILE) -I. -Os -c tuxgraphics/dnslkup.c

enc28j60.o: tuxgraphics/enc28j60.c tuxgraphics/timeout.h tuxgraphics/enc28j60.h
	$(COMPILE) -I. -Os -c tuxgraphics/enc28j60.c

ip_arp_udp_tcp.o: tuxgraphics/ip_arp_udp_tcp.c tuxgraphics/net.h tuxgraphics/enc28j60.h ip_config.h
	$(COMPILE) -I. -Os -c tuxgraphics/ip_arp_udp_tcp.c

##################
# Main source file

main.elf: tuxgraphics $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS) ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -R .eeprom -O ihex main.elf main.hex
	avr-size -C --mcu=$(DEVICE) main.elf
