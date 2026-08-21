CC = gcc
CXX = g++
CFLAGS = -Wall -g -pthread

all: hw2

# ======================================================================
# STUDENT COMPILATION BLOCK
# Choose either the C Version or the C++ Version by commenting/uncommenting.
# Do not leave both blocks uncommented at the same time.
# ======================================================================


# ----------------------------------------------------------------------
# OPTION 1: Pure C Version (main.c + crossroad.c) -- DEFAULT
# ----------------------------------------------------------------------
#hw2: main.o hw2_output.o crossroad.o
#	$(CC) $(CFLAGS) -o hw2 main.o hw2_output.o crossroad.o

#main.o: main.c crossroad.h hw2_output.h
#	$(CC) $(CFLAGS) -c main.c

#crossroad.o: crossroad.c crossroad.h hw2_output.h
#	$(CC) $(CFLAGS) -c crossroad.c


# ----------------------------------------------------------------------
# OPTION 2: C++ Version (main.cpp + crossroad.cpp)
# ----------------------------------------------------------------------
hw2: main.o hw2_output.o crossroad.o
	$(CXX) $(CFLAGS) -o hw2 main.o hw2_output.o crossroad.o
# 
main.o: main.cpp crossroad.h hw2_output.h
	$(CXX) $(CFLAGS) -c main.cpp
# 
crossroad.o: crossroad.cpp crossroad.h hw2_output.h
	$(CXX) $(CFLAGS) -c crossroad.cpp


# ======================================================================
# SHARED TARGETS
# ======================================================================

hw2_output.o: hw2_output.c hw2_output.h
	$(CC) $(CFLAGS) -c hw2_output.c

clean:
	rm -f *.o hw2