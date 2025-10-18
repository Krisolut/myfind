# -----------------------------------------
# Targets:
#   make        -> compile myfind
#   make clean  -> remove binaries
# -----------------------------------------

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
LDFLAGS :=
TARGET 	:= myfind
SRC 	:= myfind.cpp
OBJ		:= $(SRC:.cpp=.o)

LDFLAGS += -pthread
CXXFLAGS += -pthread

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean