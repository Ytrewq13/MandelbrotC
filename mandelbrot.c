#include "png_maker.h"
#include <complex.h>
#include <pthread.h>
#include <string.h>
#include "threads.h"

#define MAX_ITER 128
#define MY_INFINITY 16
#define IMG_WIDTH 1920
#define IMG_HEIGHT 1080
#define X_MIN -2.5
#define X_MAX 1.5

#define HUE_OFFSET 0

// TODO: use nproc() to determine the best number of threads
#define THREADS_CNT 16

pixel_t * get_color(double x, double y) {
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

    pixel_t * pixel = (pixel_t*)malloc(sizeof(pixel_t));
    pixel->red = (int) rgb.R;
    pixel->green = (int) rgb.G;
    pixel->blue = (int) rgb.B;
    if (iterations == MAX_ITER) {
        pixel->red = pixel->green = pixel->blue = 0;
    }
    return pixel;
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
        pixel_t *color = get_color(x_coord, y_coord);
        memcpy(pixel, color, sizeof(pixel_t));
        free(color);
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
    pthread_t threads[THREADS_CNT];
    // Create the png.
    bitmap_t image;
    int y;
    image.width = IMG_WIDTH;
    image.height = IMG_HEIGHT;

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
    struct render_thread_info thread_args[THREADS_CNT];
    int thread_row_cnt = IMG_HEIGHT / THREADS_CNT;
    for (int i = 0; i < THREADS_CNT; i++) {
        thread_args[i].img = &image;
        thread_args[i].start = i * thread_row_cnt;
        thread_args[i].end = thread_args[i].start + thread_row_cnt;
        if (IMG_HEIGHT - thread_args[i].end < thread_row_cnt)
            thread_args[i].end = IMG_HEIGHT;
        printf("Thread %02d: rows [%03d,%03d)\n", i, thread_args[i].start,
                thread_args[i].end);
    }

    for (int i = 0; i < THREADS_CNT; i++) {
        pthread_create(threads+i, NULL, render_rows, &thread_args[i]);
    }

    for (int i = 0; i < THREADS_CNT; i++) {
        pthread_join(threads[i], NULL);
    }

    save_png_to_file(&image, "image.png");
    free(image.pixels);
    return 0;
}
