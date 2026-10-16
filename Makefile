ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO nicht gesetzt)
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
CTRULIB := $(DEVKITPRO)/libctru
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -O2 -Wall -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ -I$(CTRULIB)/include
LDFLAGS := -specs=3dsx.specs $(ARCH) -L$(CTRULIB)/lib -lctru -lm

all: casino3ds.3dsx

casino3ds.elf: source/main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

casino3ds.smdh:
	smdhtool --create "Casino 3DS" "Roulette, Pferderennen, Gluecksrad" "Casino" $(CTRULIB)/default_icon.png $@

casino3ds.3dsx: casino3ds.elf casino3ds.smdh
	3dsxtool $< $@ --smdh=casino3ds.smdh

clean:
	rm -f *.elf *.smdh *.3dsx *.cia
