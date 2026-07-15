CXX ?= g++
AR ?= ar

CXXFLAGS ?= -O3 -std=c++11 -Wall -Wextra -Wpedantic

ifeq ($(OS),Windows_NT)
RM_FILES = cmd /C del /Q
RM_REDIRECT = 2>NUL
else
RM_FILES = rm -f
RM_REDIRECT =
endif

TARGET = libsbx-binary-fuse.a
OBJS = binary_fuse.o

.PHONY: default clean

default: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)

binary_fuse.o: binary_fuse.cpp binary_fuse.h
	$(CXX) $(CXXFLAGS) -c binary_fuse.cpp -o $@

clean:
	$(RM_FILES) $(TARGET) $(OBJS) $(RM_REDIRECT)
