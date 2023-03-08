# MandelbrotC

A mandelbrot set renderer written in C.

* Outputs a file `image.png` when run.
* Currently limited to 64 bit double accuracy,
* Parallel using `pthread`. Currently hard-coded to use 16 threads.
* Speed depends on the values of `MY_INFINITY` and `MAX_ITER` set at the top of `mandelbrot.c`.

## Requirements

Requires `libpng` to be installed on your machine.

## TODOs

* Implement a thread pool to maximise thread CPU time
* Dynamically determine thread count using `get_nprocs_conf()`
