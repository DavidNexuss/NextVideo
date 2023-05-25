CFLAGS= -g -I src
LDFLAGS= -lGLEW -lGL -lassimp -lglfw
LDFLAGSVK = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lassimp -lGLEW -lGL

all: bin/testGL bin/testVK

obj/gl.o: src/engine/gl.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/gl.cpp -c -o $@

obj/vk.o: src/engine/vk.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/vk.cpp -c -o $@

obj/glfwSurface.o: src/engine/glfwSurface.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/glfwSurface.cpp -c -o $@

obj/scene.o: src/engine/scene.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/scene.cpp -c -o $@

bin/testGL: obj/glfwSurface.o obj/gl.o obj/scene.o src/tests/test.cpp
	g++ $(CFLAGS) $(LDFLAGS) $^ -o $@

bin/testVK: obj/glfwSurface.o obj/vk.o obj/scene.o src/tests/test.cpp
	g++ $(CFLAGS) $(LDFLAGSVK) $^ -o $@
