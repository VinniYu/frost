# Common flags
LDFLAGS_COMMON = -lGLEW -lGL -lGLU -lglut -lstdc++
CFLAGS_COMMON  = -c -Wall -I./ -O3 -DGL_SILENCE_DEPRECATION

# Compiler
CC      = g++
CFLAGS  = ${CFLAGS_COMMON}
LDFLAGS = ${LDFLAGS_COMMON}

# Executable names
EXECUTABLE = bin/frost

# Source files
SOURCES = src/sim/main.cpp src/sim/FROST.cpp src/sim/FROST_FAST.cpp src/util/SHADER.cpp 

OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(EXECUTABLE)

# Build rules
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Generic rule for .cpp -> .o
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

# Clean rule
clean:
	rm -f src/sim/*.o $(EXECUTABLE) src/util/*.o
