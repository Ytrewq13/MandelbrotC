CC=gcc
CFLAGS=-I. -I/usr/include/SDL2 -L/usr/lib -lSDL2 -lpng -lm -D_REENTRANT -Wall -O3
DEPS = $(wildcard *.h)
OBJ := $(patsubst %.c,%.o,$(wildcard *.c))

EXE=mandelbrot
PICSDIR=out

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(EXE)
	rm $(OBJ)

run: $(EXE)
	./$(EXE)
