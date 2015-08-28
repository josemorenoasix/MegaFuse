TARGET = MegaFuse

###############
SRC = src/MegaFuseApp.cpp src/file_cache_row.cpp src/EventsHandler.cpp src/MegaFuse.cpp src/megafusemodel.cpp src/megaposix.cpp src/Config.cpp src/fuseImpl.cpp src/megacli.cpp src/Logger.cpp
SRC += sdk/megabdb.cpp sdk/megaclient.cpp sdk/megacrypto.cpp 

OUT = $(TARGET)
OBJ = $(patsubst %.cpp, %.o, $(patsubst %.c, %.o, $(SRC)))

# include directories
INCLUDES = -I inc -I /usr/local/include/cryptopp -I sdk -I /usr/local/include

# C compiler flags
CCFLAGS = -g -O2 -Wall -fstack-protector-all #-non-call-exceptions
CCFLAGS += $(shell pkg-config --cflags libcurl fuse)
CPPFLAGS =  -std=c++0x $(CCFLAGS) -D_GLIBCXX_DEBUG -D_DARWIN_C_SOURCE

# compiler
CC = gcc
CPP = g++
CXX= g++
# library paths
LIBS = 

# compile flags
LDFLAGS = -lcryptopp -lfreeimage -ldb_cxx
LDFLAGS += $(shell pkg-config --libs libcurl fuse)

.PHONY: all clean

megafuse: $(OUT)

all: megafuse

$(OUT): $(OBJ) 
	$(CPP) $(CPPFLAGS) -o $(OUT) $(OBJ) $(LDFLAGS)

.cpp.o:
	$(CPP) $(INCLUDES) $(CPPFLAGS) -c $< -o $@

clean:	
	rm -f $(OBJ) $(OUT)

install:
	cp -i $(OUT) /usr/bin/$(OUT)
	mkdir -p -m 0700 ~/.config/MegaFuse
	cp -i megafuse.conf.sample ~/.config/MegaFuse/megafuse.conf
	cp -i megafuse.service /etc/systemd/system/
	cp -i megafuse@.service /etc/systemd/system/
