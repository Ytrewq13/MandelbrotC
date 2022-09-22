#include "png_maker.h"
#include <complex.h>
#include <pthread.h>
#include <string.h>
#include "threads.h"
#include <time.h>
#include <unistd.h>

#define MAX_ITER 128
#define MY_INFINITY 16
#define IMG_WIDTH 1920 * 4
#define IMG_HEIGHT 1080 * 4
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

void *thread_spin(void *ptr)
{
    struct spin_thread_args *spin = ptr;
    int *row_ptr;
    int rows_rendered = 0;
    printf("[WORKER %02d] Worker start\n", spin->id);
    while (true) {
        queue_get(spin->q, (void**)&row_ptr);
        if (*row_ptr < 0) {
            printf("[WORKER %02d] Exiting (rendered %04d rows)...\n", spin->id, rows_rendered);
            pthread_exit(NULL);
        }
//        printf("[WORKER %02d] Rendering row %d...\n", spin->id, *row_ptr);
        render_row(*row_ptr, spin->img);
        rows_rendered++;
        free(row_ptr);
    }
    return NULL;
}

int main(int argc, char ** argv) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_t threads[nproc];

    // Create the png.
    bitmap_t image;
    image.width = IMG_WIDTH;
    image.height = IMG_HEIGHT;

    // Time how long things take
    clock_t start_time, end_time;
    clock_t duration_create_threads, duration_join_threads;

    image.pixels = calloc(image.width * image.height, sizeof(pixel_t));

    if (!image.pixels) {
        return -1;
    }

    printf("[MASTER   ] Creating worker threads...\n");
    start_time = clock();

    struct queue *task_queue = queue_init();
    struct spin_thread_args thread_args[nproc];
    for (int i = 0; i < nproc; i++) {
        thread_args[i].id = i;
        thread_args[i].q = task_queue;
        thread_args[i].img = &image;
    }

    for (int i = 0; i < nproc; i++) {
        pthread_create(threads+i, NULL, thread_spin, &thread_args[i]);
    }

    end_time = clock();
    duration_create_threads = end_time - start_time;
    printf("[MASTER   ] Created threads in %.04lf seconds\n", duration_create_threads/(double)1000000);

    printf("[MASTER   ] Constructing work queue...\n");
    int *row_task;
    for (int i = 0; i < IMG_HEIGHT; i++) {
        row_task = malloc(sizeof(int));
        *row_task = i;
        queue_add(task_queue, row_task);
    }
    for (int i = 0; i < nproc; i++) {
        row_task = malloc(sizeof(int));
        *row_task = -1;
        queue_add(task_queue, row_task);
    }

    start_time = clock();
    for (int i = 0; i < nproc; i++) {
        pthread_join(threads[i], NULL);
    }
    end_time = clock();
    duration_join_threads = end_time - start_time;
    printf("[MASTER   ] Joined threads in %ld microseconds\n", duration_join_threads);

    save_png_to_file(&image, "out/image.png");
    free(image.pixels);
    return 0;
}
