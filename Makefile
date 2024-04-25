GL_LIBS=-lglut -lGLEW -lGL

shadertoy: shadertoy.c
	gcc -Wall -Wextra -fsanitize=address -g -o $@ $< `pkg-config --libs --cflags gdk-pixbuf-2.0` $(GL_LIBS) -lm