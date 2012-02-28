# makefile, written by guido socher
#
# Please select according to the type of board you are using:
#MCU=atmega88
#DUDECPUTYPE=m88
#MCU=atmega168
#DUDECPUTYPE=m168
MCU=atmega328p
DUDECPUTYPE=m328p
#MCU=atmega644
#DUDECPUTYPE=m644
#MCU=atmega644
#DUDECPUTYPE=m644p
#
# LOADCMD in this top level Makefile is only used for reading and writing
# fuses. You have to edit the Makefiles in the subdirectories if you want
# to use the "make load" feature.
#
# === Edit this and enter the correct device/com-port:
# linux (plug in the avrusb500 and type dmesg to see which device it is):
LOADCMD=avrdude -P /dev/ttyUSB0

# mac (plug in the programer and use ls /dev/tty.usbserial* to get the name):
#LOADCMD=avrdude -P /dev/tty.usbserial-A9006MOb

# windows (check which com-port you get when you plugin the avrusb500):
#LOADCMD=avrdude -P COM4

# All operating systems: if you have set the default_serial paramter 
# in your avrdude.conf file correctly then you can just use this
# and you don't need the above -P option:
#LOADCMD=avrdude
# === end edit this
#
#
all:
	${MAKE} -C server-www-remswitch DUDECPUTYPE="$(DUDECPUTYPE)" MCU="$(MCU)" all
	${MAKE} -C server-www-simple DUDECPUTYPE="$(DUDECPUTYPE)" MCU="$(MCU)" all
	${MAKE} -C client-www-dhcp DUDECPUTYPE="$(DUDECPUTYPE)" MCU="$(MCU)" all
	${MAKE} -C client-www DUDECPUTYPE="$(DUDECPUTYPE)" MCU="$(MCU)" all
	${MAKE} -C client-tuxgr-email DUDECPUTYPE="$(DUDECPUTYPE)" MCU="$(MCU)" all
	@echo "done"
#
load: 
	@echo "Note: the load command can only be used from induvidual sub-directories. Go to the sub-directory you want and load the type of code you would like to load from there"
#
#-------------------
# Check this with make rdfuses
# 
rdfuses:
	$(LOADCMD) -p $(DUDECPUTYPE) -c stk500v2 -v -q
#
fuse:
	@echo "Setting clock source to external clock on pin xtal1"
	$(LOADCMD) -p  $(DUDECPUTYPE) -c stk500v2 -u -v -U lfuse:w:0x60:m
#
fuses:
	@echo "Setting clock source to external clock on pin xtal1"
	$(LOADCMD) -p  $(DUDECPUTYPE) -c stk500v2 -u -v -U lfuse:w:0x60:m
#
#-------------------
clean:
	${MAKE} -C server-www-remswitch clean
	${MAKE} -C server-www-simple clean
	${MAKE} -C client-www-dhcp clean
	${MAKE} -C client-www clean
	${MAKE} -C client-tuxgr-email clean
	@echo "OK: all clean"
#-------------------
