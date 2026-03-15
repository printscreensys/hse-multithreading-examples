CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
TARGET = dfs
SRCS = dfs.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
