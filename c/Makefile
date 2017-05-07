#
#
#

all: wltest

wltest: wltest.c
	cc -o wltest `pkg-config --cflags wayland-client` wltest.c `pkg-config --libs wayland-client`
