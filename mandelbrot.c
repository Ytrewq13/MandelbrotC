#include <SDL2/SDL_events.h>
#include <complex.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "png_maker.h"
#include "tpool.h"
#include "sdl_window.h"


#define MAX_ITER 128
#define MY_INFINITY 4
#define IMG_WIDTH 1280
#define IMG_HEIGHT 720
#define X_MIN -2.6
#define X_MAX 1.6

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

/* Calculate the number of iterations of c=c^2+z required before c becomes
 * unbounded
 */
int iterations(double x, double y, int max_iter)
{
    const double complex offset = x + y * I;
    if (pow(creal(offset), 2) + pow(cimag(offset), 2) >= MY_INFINITY)
        return 0;
    double complex number = offset;
    int iterations = 1;
    while (pow(creal(number), 2) + pow(cimag(number), 2) < MY_INFINITY && iterations < max_iter) {
        iterations++;
        number = cpow(number, 2) + offset;
    }
    return iterations;
}

/* Calculate and render a pixel onto the SDL Surface */
void render_pixel(double x, double y, SDL_Surface *img, int px, int py, int max_iter)
{
    int it = iterations(x, y, max_iter);
    // normalize between 0 and 360 for hue.
    double hue = 360 * (double) it / (double) max_iter;
    struct HSV hsv = {hue, 1.0, 1.0};
    struct RGB rgb = HSVToRGB(hsv);

    uint8_t red = rgb.R;
    uint8_t green = rgb.G;
    uint8_t blue = rgb.B;
    if (it == max_iter) {
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
        render_pixel(x_coord, y_coord, img, x, y, MAX_ITER);
    }
}

void render_rect(double x_min, double y_min, double w, double h, SDL_Surface *img, SDL_Rect pix_area, int max_iter)
{
    double x, y;
    for (int py = pix_area.y; py < pix_area.y + pix_area.h; py++) {
        y = y_min + (py - pix_area.y) / (double)pix_area.h * h;
        for (int px = pix_area.x; px < pix_area.x + pix_area.w; px++) {
            x = x_min + (px - pix_area.x) / (double)pix_area.w * w;
            render_pixel(x, y, img, px, py, max_iter);
        }
    }
}

void *worker_render_rect(void *args)
{
    struct {double x, y, w, h; SDL_Surface *img; SDL_Rect pix_area; int max_iter;} *arguments
        = args;
    render_rect(arguments->x, arguments->y, arguments->w, arguments->h,
            arguments->img, arguments->pix_area, arguments->max_iter);
    return NULL;
}

void *exit_thread(void *return_value)
{ pthread_exit(return_value); }

long nanos_diff(struct timespec start, struct timespec end)
{
    long retval;
    if (start.tv_nsec > end.tv_nsec) {
        /* Carry the 1 */
        end.tv_nsec += 1000000000;
        end.tv_sec -= 1;
    }
    retval = 1000000000*(end.tv_sec-start.tv_sec)+end.tv_nsec-start.tv_nsec;
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
    void (*work_func)(void *);
    void *work_args;
    printf("[WORKER %02d] Worker start\n", spin->id);
    while (true) {
        queue_get(spin->q, &work_func, &work_args);
        work_func(work_args);
    }
    return NULL;
}

void *signal_finished(void *args)
{
    pthread_cond_signal(args);
    return NULL;
}

void enqueue_render(struct queue *q, double x, double y, double w, double h, SDL_Surface *img, SDL_Rect pix_area, int max_iter, bool total)
{
    if (pix_area.w > 64) {
        SDL_Rect pix_a = {pix_area.x, pix_area.y, pix_area.w/2, pix_area.h};
        SDL_Rect pix_b = {pix_area.x+pix_area.w/2, pix_area.y, pix_area.w-pix_area.w/2, pix_area.h};
        enqueue_render(q, x, y, w/2, h, img, pix_a, max_iter, false);
        enqueue_render(q, x+w/2, y, w/2, h, img, pix_b, max_iter, false);
        return;
    }
    if (pix_area.h > 36) {
        SDL_Rect pix_a = {pix_area.x, pix_area.y, pix_area.w, pix_area.h/2};
        SDL_Rect pix_b = {pix_area.x, pix_area.y+pix_area.h/2, pix_area.w, pix_area.h-pix_area.h/2};
        enqueue_render(q, x, y, w, h/2, img, pix_a, max_iter, false);
        enqueue_render(q, x, y+h/2, w, h/2, img, pix_b, max_iter, false);
        return;
    }
    struct {double x, y, w, h; SDL_Surface *img; SDL_Rect pix_area; int
        max_iter;} *args = malloc(sizeof(*args));
    args->x = x;
    args->y = y;
    args->w = w;
    args->h = h;
    args->img = img;
    args->pix_area = pix_area;
    args->max_iter = max_iter;
    queue_add(q, &worker_render_rect, args);
//    if (total)
//        queue_add(q, &signal_finished, img->userdata);
}

int main(int argc, char ** argv) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_t threads[nproc];

    // Time how long things take
    struct timespec start, end;


    printf("[MASTER   ] Creating SDL2 window...\n");
    double y_min = (X_MAX - X_MIN) * -0.5 * IMG_HEIGHT/IMG_WIDTH;
    double y_max = (X_MAX - X_MIN) * 0.5 * IMG_HEIGHT/IMG_WIDTH;
    struct sdl_window_info window = my_sdl_init(X_MIN, y_min, X_MAX-X_MIN,
            y_max-y_min, IMG_WIDTH, IMG_HEIGHT, MAX_ITER);

    printf("[MASTER   ] Creating worker threads...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    struct queue *task_queue = queue_init();
    struct spin_thread_args thread_args[nproc];
    for (int i = 0; i < nproc; i++) {
        thread_args[i].id = i;
        thread_args[i].q = task_queue;
        thread_args[i].img_surf = window.surf;
        thread_args[i].keep_window_open = &window.keep_open;
    }

    for (int i = 0; i < nproc; i++) {
        pthread_create(threads+i, NULL, worker_spin, &thread_args[i]);
    }
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Created threads in %.04lf seconds\n", nanos_diff(start, end)/(double)1000000000);

    printf("[MASTER   ] Constructing work queue...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Created work queue in %.04lf seconds\n", nanos_diff(start, end)/(double)1000000000);

    long eventloop_i = 0;
    while (window.keep_open) {
        if (eventloop_i == 0) {
            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
        }
        SDL_Event e;
        while (SDL_PollEvent(&e) > 0) {
            switch (e.type) {
                case SDL_QUIT:
                    window.keep_open = false;
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_q:
                            window.keep_open = false;
                            break;
                        case SDLK_r:
                            window = my_sdl_init(X_MIN, y_min, X_MAX-X_MIN,
                                    y_max-y_min, IMG_WIDTH, IMG_HEIGHT,
                                    MAX_ITER);
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_i:
                            window.v.x += window.v.w / 4;
                            window.v.y += window.v.h / 4;
                            window.v.w /= 2;
                            window.v.h /= 2;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_o:
                            window.v.w *= 2;
                            window.v.h *= 2;
                            window.v.x -= window.v.w / 4;
                            window.v.y -= window.v.h / 4;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_a:
                            window.v.x -= window.v.w / 4;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_d:
                            window.v.x += window.v.w / 4;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_w:
                            window.v.y -= window.v.h / 4;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_s:
                            window.v.y += window.v.h / 4;
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_UP:
                            window.max_iter *= 2;
                            printf("[MASTER   ] Using %d iterations\n", window.max_iter);
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                        case SDLK_DOWN:
                            window.max_iter /= 2;
                            printf("[MASTER   ] Using %d iterations\n", window.max_iter);
                            sdl_blank_screen(window);
                            SDL_UpdateWindowSurface(window.win);
                            enqueue_render(task_queue, window.v.x, window.v.y, window.v.w, window.v.h, window.surf, window.v.view, window.max_iter, true);
                            break;
                    }
                    break;
            }
        }
        SDL_UpdateWindowSurface(window.win);
        SDL_Delay(33);
        eventloop_i++;
    }

    for (int i = 0; i < nproc; i++) {
        queue_add(task_queue, exit_thread, NULL);
    }
    printf("[MASTER   ] Waiting for workers to finish...\n");
    clock_gettime(CLOCK_REALTIME, &start);
//    struct rendering_stats *stats_tmp, stats_total;
//    stats_total.nanos_rendering = 0;
//    stats_total.nanos_waiting = 0;
    for (int i = 0; i < nproc; i++) {
//        pthread_join(threads[i], (void*)&stats_tmp);
        pthread_join(threads[i], NULL);
//        stats_total.nanos_rendering += stats_tmp->nanos_rendering;
//        stats_total.nanos_waiting += stats_tmp->nanos_waiting;
    }
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Workers finished in %.04lf seconds\n", nanos_diff(start, end)/(double)1000000000);
//    printf("[MASTER   ] Workers spent a total of %.04lf seconds rendering and %.04lf seconds waiting\n", stats_total.nanos_rendering/(double)1000000000, stats_total.nanos_waiting/(double)1000000000);

//    SDL_UpdateWindowSurface(window);
//    SDL_Delay(500);

//    save_png_to_file(&image, "out/image.png");
    return 0;
}
