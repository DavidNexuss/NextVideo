CFLAGS= -g -I src
LDFLAGS= -lGLEW -lGL -lz
LDFLAGSVK = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lassimp -lGLEW -lGL -lz

all: bin/testGL bin/testVK
LIBS= libDist/glfw/src/libglfw3.a libDist/assimp/lib/libassimp.a

obj/gl.o: src/engine/gl.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/gl.cpp -c -o $@

obj/vk.o: src/engine/vk.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/vk.cpp -c -o $@

obj/glfwSurface.o: src/engine/glfwSurface.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/glfwSurface.cpp -c -o $@

obj/scene.o: src/engine/scene.cpp src/engine/engine.hpp
	g++ $(CFLAGS) src/engine/scene.cpp -c -o $@

bin/testGL: obj/glfwSurface.o obj/gl.o obj/scene.o src/tests/test.cpp $(LIBS)
	g++ $(CFLAGS) $(LDFLAGS) $^ -o $@

bin/testVK: obj/glfwSurface.o obj/vk.o obj/scene.o src/tests/test.cpp $(LIBS)
	g++ $(CFLAGS) $(LDFLAGSVK) $^ -o $@

libDist/assimp: lib/assimp/CMakeLists.txt
	cmake lib/assimp -DBUILD_SHARED_LIBS=OFF  -B libDist/assimp

libDist/glfw: lib/glfw/CMakeLists.txt
	cmake lib/glfw -B libDist/glfw

libDist/glfw/src/libglfw3.a: libDist/glfw
	cd libDist/glfw; make -j4

libDist/assimp/lib/libassimp.a: libDist/assimp
	cd libDist/assimp; make -j4

glfw: libDist/glfw/src/libglfw3.a

assimp: libDist/assimp/lib/libassimp.a
