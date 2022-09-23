#include <complex.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "png_maker.h"
#include "threads.h"


#define MAX_ITER 128
#define MY_INFINITY 16
#define IMG_WIDTH 1280
#define IMG_HEIGHT 720
#define X_MIN -2.5
#define X_MAX 1.5

#define HUE_OFFSET 0

/* TODO:
 * * Rework thread pool to run arbitrary rendering function
 *      * Render row (existing function)
 *      * Render dynamic rectangle - to allow rendering center of image first
 *      * Function which calls pthread_exit()
 * * Use SDL2 to display the rendered image
 *      * Interactive controls to move display window (click & drag)
 *      * Re-render the image on-the-fly using the thread-pool (only re-render
 *      new regions)
 * * Arbitrary-precision math
 */

void calculate_color(double x, double y, SDL_Surface *img, int px, int py)
{
    // calculate the color of a pixel with complex value x, y
    double complex number = 0;
    const double complex offset = x + y * I;
    int iterations = 0;
    while (pow(creal(number), 2) + pow(cimag(number), 2) < MY_INFINITY &&
            iterations < MAX_ITER) {
        iterations++;
        number = cpow(number, 2) + offset;
    }

    // normalize between 0 and 360 for hue.
    double hue = 360 * (double) iterations / (double) MAX_ITER;
    struct HSV hsv = {hue, 1.0, 1.0};
    struct RGB rgb = HSVToRGB(hsv);

    uint8_t red = rgb.R;
    uint8_t green = rgb.G;
    uint8_t blue = rgb.B;
    if (iterations == MAX_ITER) {
        red = green = blue = 0;
    }
    uint32_t pixel = red << 16 | green << 8 | blue;
    uint32_t *target_pixel = (uint32_t*) ((uint8_t*) img->pixels + py*img->pitch + px*img->format->BytesPerPixel);
    *target_pixel = pixel;
}

void render_row(int y, SDL_Surface *img)
{
    double min_y = (X_MAX - X_MIN) * -0.5 * img->h/img->w;
    double y_range = (X_MAX - X_MIN) * 0.5 * img->h/img->w - min_y;
    double x_coord, y_coord;
    y_coord = ((double)y / (double)img->h) * y_range + min_y;
    for (int x = 0; x < img->w; x++) {
        x_coord = ((double)x / (double)img->w) * (X_MAX - X_MIN) + X_MIN;
        calculate_color(x_coord, y_coord, img, x, y);
    }
}

long nanos_diff(struct timespec start, struct timespec end)
{
    long retval;
    bool adjust = false;
    if (start.tv_nsec > end.tv_nsec) {
        adjust = true;
        end.tv_nsec += 1000000000;
        end.tv_sec -= 1;
    }
    retval = 1000000000*(end.tv_sec-start.tv_sec)+end.tv_nsec-start.tv_nsec;
    if (retval < 0)
        printf("Retval is negative: %ld (adjust: %d)\n", retval, adjust);
    return retval;
}

struct rendering_stats {
    long nanos_rendering;
    long nanos_waiting;
};

void *worker_spin(void *ptr)
{
    /* TODO: run arbitrary work function passed via the queue. */
    struct spin_thread_args *spin = ptr;
    int *row_ptr;
    int rows_rendered = 0;
    long wait_ns = 0, render_ns = 0;
    struct rendering_stats *stats;
    struct timespec start, end;
    printf("[WORKER %02d] Worker start\n", spin->id);
    while (true) {
        clock_gettime(CLOCK_REALTIME, &start);
        queue_get(spin->q, (void**)&row_ptr);
        clock_gettime(CLOCK_REALTIME, &end);
        wait_ns += nanos_diff(start, end);
        if (*row_ptr < 0) {
            stats = malloc(sizeof(struct rendering_stats));
            stats->nanos_rendering = render_ns;
            stats->nanos_waiting = wait_ns;
            printf("[WORKER %02d] Exiting (rendered %04d rows in %.04lf seconds"
                    ", waited %.04lf seconds)...\n", spin->id, rows_rendered,
                    render_ns/(double)1000000000,
                    wait_ns/(double)1000000000);
            if (queue_empty(spin->q))
                *spin->keep_window_open = false;
            pthread_exit(stats);
        }
        clock_gettime(CLOCK_REALTIME, &start);
        render_row(*row_ptr, spin->img_surf);
        clock_gettime(CLOCK_REALTIME, &end);
        render_ns += nanos_diff(start, end);
        rows_rendered++;
        free(row_ptr);
    }
    return NULL;
}

void render_image(struct queue *q, double x, double y, double w, double h)
{
    // TODO: enqueue all the render tasks
    // TODO: implement a way of waiting for the tasks - a pthread_cond?
}

int main(int argc, char ** argv) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_t threads[nproc];

    // Time how long things take
    struct timespec start, end;


    printf("[MASTER   ] Creating SDL2 window...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "ERROR: Failed to initialise the SDL2 library\n");
        exit(EXIT_FAILURE);
    }
    SDL_Window *window = SDL_CreateWindow("SDL window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, IMG_WIDTH, IMG_HEIGHT, 0);
    bool keep_window_open = true;
    if (window == NULL) {
        fprintf(stderr, "ERROR: failed to create window\n");
        exit(EXIT_FAILURE);
    }
    SDL_Surface *window_surface = SDL_GetWindowSurface(window);
    if (window_surface == NULL) {
        fprintf(stderr, "ERROR: failed to get surface from the window\n");
        exit(EXIT_FAILURE);
    }

    printf("[MASTER   ] Creating worker threads...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    struct queue *task_queue = queue_init();
    struct spin_thread_args thread_args[nproc];
    for (int i = 0; i < nproc; i++) {
        thread_args[i].id = i;
        thread_args[i].q = task_queue;
        thread_args[i].img_surf = window_surface;
        thread_args[i].keep_window_open = &keep_window_open;
    }

    for (int i = 0; i < nproc; i++) {
        pthread_create(threads+i, NULL, worker_spin, &thread_args[i]);
    }
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Created threads in %.04lf seconds\n", (end.tv_sec+end.tv_nsec/(double)1000000000 - (start.tv_sec+start.tv_nsec/(double)1000000000)));

    printf("[MASTER   ] Constructing work queue...\n");
    clock_gettime(CLOCK_REALTIME, &start);
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
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Created work queue in %lf seconds\n", (end.tv_sec+end.tv_nsec/(double)1000000000 - (start.tv_sec+start.tv_nsec/(double)1000000000)));

    while (keep_window_open) {
        SDL_Event e;
        while (SDL_PollEvent(&e) > 0) {
            switch (e.type) {
                case SDL_QUIT:
                    keep_window_open = false;
                    break;
            }
        }
        SDL_UpdateWindowSurface(window);
        SDL_Delay(33);
    }

    printf("[MASTER   ] Waiting for workers to finish...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    struct rendering_stats *stats_tmp, stats_total;
    stats_total.nanos_rendering = 0;
    stats_total.nanos_waiting = 0;
    for (int i = 0; i < nproc; i++) {
        pthread_join(threads[i], (void*)&stats_tmp);
        stats_total.nanos_rendering += stats_tmp->nanos_rendering;
        stats_total.nanos_waiting += stats_tmp->nanos_waiting;
    }
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Workers finished in %.04lf seconds\n", (end.tv_sec+end.tv_nsec/(double)1000000000 - (start.tv_sec+start.tv_nsec/(double)1000000000)));
    printf("[MASTER   ] Workers spent a total of %.04lf seconds rendering and %.04lf seconds waiting\n", stats_total.nanos_rendering/(double)1000000000, stats_total.nanos_waiting/(double)1000000000);

//    save_png_to_file(&image, "out/image.png");
    return 0;
}
