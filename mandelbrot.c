#include "png_maker.h"
#include <complex.h>
#include <pthread.h>
#include <string.h>
#include "threads.h"
#include <time.h>
#include <unistd.h>

#define MAX_ITER 128
#define MY_INFINITY 16
#define IMG_WIDTH 3840
#define IMG_HEIGHT 2160
#define X_MIN -2.5
#define X_MAX 1.5

#define HUE_OFFSET 0

void calculate_color(double x, double y, pixel_t *pixel)
{
    // calculate the color of a pixel with complex value x, y
    double complex number = 0;
    const double complex offset = x + y * I;
    int iterations = 0;
    while (pow(creal(number), 2) + pow(cimag(number), 2) < MY_INFINITY && iterations < MAX_ITER) {
        iterations++;
        number = cpow(number, 2) + offset;
    }

    // normalize between 0 and 360 for hue.
    double hue = 360 * (double) iterations / (double) MAX_ITER;
    struct HSV hsv = {hue, 1.0, 1.0};
    struct RGB rgb = HSVToRGB(hsv);

    pixel->red = (int) rgb.R;
    pixel->green = (int) rgb.G;
    pixel->blue = (int) rgb.B;
    if (iterations == MAX_ITER) {
        pixel->red = pixel->green = pixel->blue = 0;
    }
}

void render_row(int y, bitmap_t *img)
{
    double min_y = (X_MAX - X_MIN) * -0.5 * img->height/img->width;
    double y_range = (X_MAX - X_MIN) * 0.5 * img->height/img->width - min_y;
    double x_coord, y_coord;
    y_coord = ((double)y / (double)img->height) * y_range + min_y;
    for (int x = 0; x < img->width; x++) {
        pixel_t *pixel = pixel_at(img, x, y);
        x_coord = ((double)x / (double)img->width) * (X_MAX - X_MIN) + X_MIN;
        calculate_color(x_coord, y_coord, pixel);
    }
}

void *render_rows(void *ptr)
{
    struct render_thread_info *info = ptr;
    for (int y = info->start; y < info->end; y++)
        render_row(y, info->img);
    return NULL;
}

int main(int argc, char ** argv) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_t threads[nproc];
    int threads_busy[nproc];
    int threads_avail = nproc;

    // Create the png.
    bitmap_t image;
    int y;
    image.width = IMG_WIDTH;
    image.height = IMG_HEIGHT;

    // Time how long things take
    clock_t start_time, end_time;
    clock_t duration_create_args, duration_create_threads,
            duration_join_threads;

    image.pixels = calloc(image.width * image.height, sizeof(pixel_t));

    if (!image.pixels) {
        return -1;
    }

    /* TODO: Use a thread pool
     * - The middle of the mandelbrot set takes much longer to compute than the
     *   top and bottom.
     * - If we create 16 threads and then give them each a region of the set,
     *   the threads at the top and bottom will finish early and be waiting a
     *   while for the middle of the set to be rendered.
     * - We should create 16 threads, then give them each a single row to
     *   render. When a thread is finished, we reuse it to compute the next
     *   row.
     */
    start_time = clock();
    struct render_thread_info thread_args[nproc];
    int thread_row_cnt = IMG_HEIGHT / nproc;
    for (int i = 0; i < nproc; i++) {
        threads_busy[i] = 1;
        thread_args[i].img = &image;
        thread_args[i].start = i * thread_row_cnt;
        thread_args[i].end = thread_args[i].start + thread_row_cnt;
        if (IMG_HEIGHT - thread_args[i].end < thread_row_cnt)
            thread_args[i].end = IMG_HEIGHT;
        printf("Thread %02d: rows [%03d,%03d)\n", i, thread_args[i].start,
                thread_args[i].end);
    }
    end_time = clock();
    duration_create_args = end_time - start_time;
    printf("Finished creating thread args in %ld microseconds\n", duration_create_args);

    start_time = clock();
    for (int i = 0; i < nproc; i++) {
        pthread_create(threads+i, NULL, render_rows, &thread_args[i]);
    }
    end_time = clock();
    duration_create_threads = end_time - start_time;
    printf("Finished creating threads in %ld microseconds\n", duration_create_threads);

    for (int i = 0; i < nproc; i++) {
        start_time = clock();
        pthread_join(threads[i], NULL);
        end_time = clock();
        duration_join_threads = end_time - start_time;
        printf("Joined thread %d in %ld microseconds\n", i, duration_join_threads);
    }

    save_png_to_file(&image, "out/image.png");
    free(image.pixels);
    return 0;
}
