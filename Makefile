GL_LIBS=-lglfw -lGLEW -lGL

shadertoy.bin: ./prog/shadertoy.c
	gcc -Wall -Wextra -static-libasan -fsanitize=address -g -o $@ $< `pkg-config --libs --cflags gdk-pixbuf-2.0` $(GL_LIBS) -lm