all: test

test: main.c
	cc -g -O2 -Wall -Wshadow -o test `pkg-config --cflags gtk+-3.0 egl wayland-egl glesv2` main.c `pkg-config --libs gtk+-3.0 egl wayland-egl glesv2` -lm

clean:
	rm -f test
