CXXFLAGS+=-std=c++17 -O3
CXX=g++

all: out out/main.o

run: out out/main.o
		./out/main.o

out/main.o: src/main.cpp
		$(CXX) $^ $(CXXFLAGS) -o out/main.o
		
out: 
		mkdir out

clean: 
		rm -rf out