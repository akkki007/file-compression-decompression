CXX = g++
CXXFLAGS = -std=c++17 -O2 -I./Crow/include -pthread
LDFLAGS = -pthread

TARGET = server

all: $(TARGET)

$(TARGET): server.cpp huffman.h lzw.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) server.cpp $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
