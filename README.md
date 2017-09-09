A cool mandelbrot set generator witten in C.
Outputs a file "image.png" when run.
Currently limited to 64 bit double accuracy,
does not parallelize at all (yet), so it will be quite slow.
Speed depends on the values of
MY\_INFINITY and MAX\_ITER
set at the top of "mandelbrot.c".
Requires libpng to be installed on your machine.
