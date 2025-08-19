APP := chess_rl
SRC_DIR := src
INC_DIR := include
OBJ_DIR := build

CXX ?= clang++
STD := c++17

# Optimize for aarch64/Termux
CXXFLAGS := -std=$(STD) -O3 -Ofast -flto -pipe -fno-exceptions -fno-rtti -DNDEBUG \
    -march=armv8-a -mtune=cortex-a76 -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter -I$(INC_DIR)
LDFLAGS := -flto

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

.PHONY: all clean run dirs

all: dirs $(APP)

dirs:
	mkdir -p $(OBJ_DIR)
	mkdir -p data

$(APP): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(APP)

clean:
	rm -rf $(OBJ_DIR) $(APP)
