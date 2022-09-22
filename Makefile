CC=gcc
CFLAGS=-I. -lpng -lm
DEPS = $(wildcard *.h)
OBJ := $(patsubst %.c,%.o,$(wildcard *.c))

EXE=mandelbrot
PICSDIR=out

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(EXE) $(PICSDIR)/*.png
	rm $(OBJ)

run: $(EXE)
	./$(EXE)
