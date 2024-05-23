#!/bin/sh

set -x

cflags="-march=native -O3 -g3 -Wall "
cflags="$cflags $(pkg-config --cflags glfw3 gl)"
ldflags=$(pkg-config --static --libs glfw3 gl)

cc $cflags main.c $ldflags -o mandelbrot
