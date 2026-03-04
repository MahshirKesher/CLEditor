CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23
INCLUDES = -I ./src

SRC = \
./main.cpp \
./core.cpp \
./terminal.cpp \
./cursor.cpp \
./input.cpp

TARGET = eclair

v3: $(SRC)
	$(CXX) $(SRC) $(CXXFLAGS) $(INCLUDES) -o $(TARGET)
