# Common flags
LDFLAGS_COMMON = -lGLEW -lGL -lGLU -lglut -lstdc++
CFLAGS_COMMON  = -c -Wall -I./ -O3 -DGL_SILENCE_DEPRECATION

# Compiler
CC      = g++
CFLAGS  = ${CFLAGS_COMMON}
LDFLAGS = ${LDFLAGS_COMMON}

# executable names
SIMULATOR = bin/frost
RENDERER = bin/render

# Source files
SIMULATOR_SRC = src/sim/main.cpp src/sim/FROST.cpp src/sim/FROST_FAST.cpp src/util/SHADER.cpp 
RENDERER_SRC = src/util/RENDER_DENSITY.cpp src/util/SHADER.cpp

SIMULATOR_OBJ = $(SIMULATOR_SRC:.cpp=.o)
RENDERER_OBJ = $(RENDERER_SRC:.cpp=.o)

# Default target
all: $(SIMULATOR) $(RENDERER)

# Build rules
$(SIMULATOR): $(SIMULATOR_OBJ)
	$(CC) $(SIMULATOR_OBJ) $(LDFLAGS) -o $@

# Renderer build
$(RENDERER): $(RENDERER_OBJ)
	$(CC) $(RENDERER_OBJ) $(LDFLAGS) -o $@

# Generic rule for .cpp -> .o
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

# Clean rule
clean:
	rm -f src/sim/*.o src/util/*.o $(SIMULATOR) $(RENDERER)
