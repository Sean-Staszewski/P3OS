CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g

LIB_NAME = libWad.a
LIB_SRC  = Wad.cpp
LIB_OBJ  = Wad.o

all: $(LIB_NAME)

# Build static library
$(LIB_NAME): $(LIB_OBJ)
	ar rcs $(LIB_NAME) $(LIB_OBJ)

# Compile object file
$(LIB_OBJ): Wad.cpp Wad.h
	$(CXX) $(CXXFLAGS) -c Wad.cpp -o Wad.o

clean:
	rm -f $(LIB_OBJ) $(LIB_NAME)

.PHONY: all clean
