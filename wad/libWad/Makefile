CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++17

SRC = Wad.cpp wad_dump.cpp
OBJ = $(SRC:.cpp=.o)

all: wad_dump

wad_dump: $(OBJ)
	$(CXX) $(CXXFLAGS) -o wad_dump $(OBJ)

%.o: %.cpp Wad.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o wad_dump
