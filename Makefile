# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -lvirt

# Targets
TARGET = vm_manager
SOURCES = main.cpp vm.cpp
HEADERS = vm.hpp
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

# Compile individual source files into objects
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild the project from scratch
rebuild: clean all
