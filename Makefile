.phony: all clean test

all: build_dir builder

build_dir:
	mkdir -p build

builder: builder.cpp
	g++ -o build/builder builder.cpp --std=c++17 -lstdc++fs

clean:
	rm -rf build

test:
	./build/builder
