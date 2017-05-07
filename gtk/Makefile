all: test

test: main.c
	cc -o test `pkg-config --cflags gtk+-3.0 egl wayland-egl` main.c `pkg-config --libs gtk+-3.0 egl wayland-egl`
