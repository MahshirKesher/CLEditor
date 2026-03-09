CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23
INCLUDES = -I ./

SRC = \
./main.cpp \
./core.cpp \
./terminal.cpp \
./cursor.cpp \
./input.cpp \
./viewport.cpp \
./render.cpp \
./text.cpp \
./file.cpp

TARGET = eclair

v3: $(SRC)
	$(CXX) $(SRC) $(CXXFLAGS) $(INCLUDES) -o $(TARGET)
