CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -Iinclude $(shell sdl2-config --cflags)
LIBS = $(shell sdl2-config --libs) -lSDL2_mixer -lGL -lGLU -Wl,-rpath,'$$ORIGIN/libs'

SRCS = src/main.cpp \
       src/game_data.cpp \
       src/glinterface.cpp \
       src/camera.cpp \
       src/piece.cpp \
       src/plane.cpp \
       src/ship.cpp \
       src/player.cpp \
       src/enemy.cpp \
       src/miniature.cpp \
       src/terrain.cpp \
       src/button.cpp \
       src/stage_interface.cpp \
       src/sound_mixer.cpp \
       src/save_data.cpp \
       src/custom_stage.cpp \
       src/menu_battle.cpp \
       src/controller_input.cpp \
       src/touch_input.cpp \
       src/split_screen_menu.cpp \
       src/split_screen_match.cpp \
       src/neutron.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = build/neutron

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p build
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) build/neutron neutron

.PHONY: all clean
