CFLAGS= -O3 -I src
LDFLAGS= -lGLEW -lGL -lassimp -lglfw
all: bin/test
obj/engine.o: src/engine/engine.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/engine.cpp -c -o $@

bin/test: obj/engine.o src/tests/test.cpp
	g++ $(CFLAGS) $(LDFLAGS) $^ -o $@
