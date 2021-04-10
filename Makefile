#PROFILE=YES
PROFILE=APPLY

CHAINPREFIX=/opt/trimui-toolchain
CROSS_COMPILE=$(CHAINPREFIX)/bin/arm-buildroot-linux-gnueabi-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip
SYSROOT     := $(CHAINPREFIX)/arm-buildroot-linux-gnueabi/sysroot
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS 		= -O2 -marm -march=armv5te -mtune=arm926ej-s
CFLAGS		+= -fdata-sections -ffunction-sections -fno-PIC
CFLAGS		+= -flto -fomit-frame-pointer -fno-builtin -fno-strict-aliasing
CFLAGS		+= -Wno-write-strings -Wno-sign-compare
CFLAGS		+= $(SDL_CFLAGS) 
CFLAGS		+= -I./main/emu -I./main/sdl -I./main/headers ${DEFINES}
CFLAGS		+= -DNOROMLOADER
DEFINES 	= -DTRIMUI
LDFLAGS 	= -no-pie -lSDL
# LDFLAGS		+= -Wl,--as-needed
LDFLAGS		+= -Wl,--gc-sections -flto -s
LDFLAGS		+= -lSDL_image -lSDL_ttf -ldl

ifeq ($(PROFILE), YES)
OUT	 		= oswan_pm
CFLAGS += -fprofile-generate -fprofile-dir=/mnt/SDCARD/profile/oswan
LDFLAGS += -lgcov -fprofile-generate -fprofile-dir=/mnt/SDCARD/profile/oswan
else
OUT	 		= oswan
ifeq ($(PROFILE), APPLY)
CFLAGS += -fprofile-use -fprofile-dir=./profile -fbranch-probabilities
endif
endif

SDL 		= main/sdl/main.c main/sdl/menu.c main/sdl/input.c main/sdl/game_input.c
CPU 		= main/emu/cpu/nec.c
CORE 		= main/emu/WS.c main/emu/WSFileio.c main/emu/WSRender.c
DRAWING 	= main/sdl/gui_drawing.c main/sdl/drawing.c

# Sound support
CORE 	   += main/emu/WSApu.c 
DEFINES    += -DSOUND_ON -DSOUND_EMULATION -DAUDIOFRAMESKIP
# Enable this to support zip files
# Here, Support for zips is enabled
#CFLAGS 	   +=-DZIP_SUPPORT -I./minizip
#LDFLAGS	   +=-lz
#THIRD_PARTY = minizip/unzip.o minizip/ioapi.o

SOURCES 	= ${SDL} ${CPU} ${CORE} ${DRAWING} 
SOURCES	   += ${THIRD_PARTY}

OBJS 		= ${SOURCES:.c=.o}

all		: ${OUT}

${OUT}	: ${OBJS}
		${CC} -o ${OUT} ${SOURCES} ${CFLAGS} ${LDFLAGS}
	
clean	:
		rm ${OBJS} ${OUT}
