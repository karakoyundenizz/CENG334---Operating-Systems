CXX = g++
CXXFLAGS = -Wall -O2 -Werror=unreachable-code -Werror=return-type
TARGET = mergeext2fs
SRCS = hw3.cpp ext2fs_print.c
HDRS = ext2fs.h ext2fs_print.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
