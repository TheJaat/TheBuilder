.PHONY: all clean tests

all: build_dir builder tests

build_dir:
	mkdir -p build

builder: builder.cpp
	g++ -o build/builder builder.cpp --std=c++17 -lstdc++fs

clean:
	rm -rf build
	$(MAKE) -C tests clean

tests:
	$(MAKE) -C tests
