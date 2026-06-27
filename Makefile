# Compiler configuration
CXX = g++
# We MUST use c++17 for the shared_mutex, and -pthread for multithreading
CXXFLAGS = -std=c++17 -pthread -Wall -O2

# Output binary name
TARGET = server

# Source files
SRCS = server.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)