CC = g++
CFLAGS = -std=c++17 -Wall -D_FILE_OFFSET_BITS=64 -I../libWad
LDFLAGS = -lfuse

SRCS = wadfs.cpp ../libWad/Wad.cpp

all: wadfs

wadfs: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f wadfs

.PHONY: all clean
