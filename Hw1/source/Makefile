# Define the compiler
CXX = g++

# Define the compiler flags (C++11/17 standard, enable warnings, optimize)
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# The default target that runs when you just type 'make'
all: merger

# How to build the 'merger' executable
merger: merger_skeleton.cpp merger_parser.c
	$(CXX) $(CXXFLAGS) -o merger merger_skeleton.cpp merger_parser.c

# A cleanup rule to remove generated files
clean:
	rm -f merger *.o