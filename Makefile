##########################################################################
##########################################################################
#
# Main make file for YAPT, plugins and player
#
# - depends:
#		libexpat, glfw
#
# Compiler settings

CC     = clang++

OBJDIR 	 = obj

ROOT		 = .
EXT_ROOT	 = $(ROOT)/ext
BASS_DIR     = $(EXT_ROOT)/bass

SDK = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk


INCLUDE_FILES = -I/opt/local/include/freetype2 -I/opt/local/include -I/usr/local/include -I. -I$(BASS_DIR) 
LIB_FILES = -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo -L/opt/local/lib -lGLEW -lglfw -lexpat -lfreetype



CPPFLAGS = -g -arch x86_64 -stdlib=libc++ -std=c++11
CPPFLAGS += -isysroot $(SDK)
CPPFLAGS += $(INCLUDE_FILES)

CFLAGS = -g -arch x86_64 -std=c99
CFLAGS += -isysroot $(SDK)
CFLAGS += $(INCLUDE_FILES)

# Linker settings
LD = clang++
LDFLAGS =  -dynamiclib -arch x86_64 -stdlib=libc++
LDFLAGS += $(LIB_FILES)

LINK_LIBS += -framework Cocoa -framework OpenGL -L/opt/local/lib -lglfw3 -lexpat
NP_LINK_LIBS = 
PLAYER_LINK_LIBS += $(LIB_FILES)
PLAYER_LINK_LIBS += $(BASS_DIR)/libbass.dylib
BASS_LIB = $(BASS_DIR)/libbass.dylib

PLAYER_SRC_FILES = \
	renderer.cpp \

PLAYER_OBJ_FILES := $(patsubst %.cpp,%.o,$(PLAYER_SRC_FILES))

# Default: Build all tests
all: player

 %.o : %.cpp
	$(CC) -c $(CPPFLAGS) $< -o $@

%.o : %.c
	$(CC) -c $(CFLAGS)  $< -o $@


player: $(PLAYER_OBJ_FILES)
	$(CC) $(CFLAGS) $(PLAYER_OBJ_FILES) $(PLAYER_LINK_LIBS) -o player

clean:
	rm $(PLAYER_OBJ_FILES) layer

