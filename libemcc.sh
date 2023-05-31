#!/bin/sh

CC="emcc -O3"
AR=emar

echo "Creating imgui implementation..."
$CC -I lib/imgui -c lib/imgui/backends/imgui_impl_opengl3.cpp -o libDist/imgui/imgui_impl_opengl3.o
$CC -I lib/imgui -c lib/imgui/backends/imgui_impl_glfw.cpp -o libDist/imgui/imgui_impl_glfw.o

echo "Creating imgui..."
$CC -c lib/imgui/*.cpp
$AR r imgui.a *.o
rm *.o

mv imgui.a libDist/imgui/
