CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g

# Path to the library (relative to this Makefile)
LIBDIR = ../wad/libWad
LIB = $(LIBDIR)/libWad.a

# Test programs
TESTS = wad_dump libtest

all: $(LIB) $(TESTS)

# Build the library by calling its Makefile
$(LIB):
	$(MAKE) -C $(LIBDIR)

# Build wad_dump
wad_dump: $(LIB)
	$(CXX) $(CXXFLAGS) -I$(LIBDIR) \
	    wad_dump.cpp \
	    -L$(LIBDIR) -lWad \
	    -o wad_dump

# Build libtest (GoogleTest)
libtest: $(LIB)
	$(CXX) $(CXXFLAGS) -I$(LIBDIR) \
	    libtest.cpp \
	    -L$(LIBDIR) -lWad \
	    -lgtest -lgtest_main -pthread \
	    -o libtest

clean:
	$(MAKE) -C $(LIBDIR) clean
	rm -f $(TESTS)
