CXX = g++ 
CXXFLAGS = -Wall -std=gnu++11 -p

all : 
	$(CXX) main.cpp $(CXXFLAGS) $(shell sdl-config --cflags --libs) -o chip
