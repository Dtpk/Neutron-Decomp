CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -Iinclude $(shell sdl2-config --cflags)
LIBS = $(shell sdl2-config --libs) -lGL -lGLU

SRCS = src/main.cpp \
       src/editor.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = build/neutron_map_editor

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p build
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
