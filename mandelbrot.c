#include <complex.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <immintrin.h> // TODO
#include <mpfr.h>

#include "png_maker.h"
#include "tpool.h"
#include "sdl_window.h"


#define MAX_ITER 128
#define MY_INFINITY 4
#define IMG_WIDTH 1920
#define IMG_HEIGHT 1080
#define X_MIN -2.6
#define X_MAX 1.6

#define HUE_OFFSET 0

/* TODO:
 * * Fixed-point numbers - could they be faster than MPFR fixed-width floating
 * point?
 * * Would OpenCL be faster for computing the mandelbrot iterations?
 *      * OpenCL C mixed-precision (MPFR)?
 * * When zooming, use SDL_BlitScaled
 * * Don't re-render areas which already have been determined to terminate (when changing the max iterations)
 * * Write rendering function using AVX2 256-bit SIMD compiler intrinsics (immintrin.h)
 */

void render_rect(double x, double y, double w, double h, SDL_Surface *img,
        SDL_Rect view, int max_iter)
{
    double scale_x = w / (double)view.w;
    double scale_y = h / (double)view.h;
    double x_cur, y_cur = y;
    double z_real, z_imag;
    int it;
    for (int py = view.y; py < view.y + view.h; py++) {
        x_cur = x;
        for (int px = view.x; px < view.x + view.w; px++) {
            it = 0;  /* Iterations counter */
            z_real = x_cur;
            z_imag = y_cur;
            while (pow(z_real, 2) + pow(z_imag, 2) < MY_INFINITY && it < max_iter) {
                it++;
                /* z = cpow(z, 2) + c
                 * z = (a + bI)(a + bI) + (d + eI)
                 * z = a^2 + 2*a*bI - b^2 + d + eI
                 * z_real = a^2 - b^2 + d
                 * z_imag = 2*a*b + e
                 * z_real = z_real^2 - z_imag^2 + x
                 * z_imag = 2*z_real*z_imag + y
                 */
                double a = pow(z_real, 2) - pow(z_imag, 2) + x_cur;
                double b = 2 * z_real * z_imag + y_cur;
                z_real = a;
                z_imag = b;
            }
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
            x_cur += scale_x;
        }
        y_cur += scale_y;
    }
}

void render_rect_high_precision(mpfr_t x, mpfr_t y, mpfr_t w, mpfr_t h,
        SDL_Surface *img, SDL_Rect view, int max_iter, long precision)
{
    mpfr_t scale_x, scale_y;
    mpfr_t x_cur, y_cur;
    mpfr_t z_real, z_imag, mpfr_tmp1, mpfr_tmp2, z_abs_2;
    int it;

    // TODO: determine if there are any black pixels in the region described by `view`
    mpfr_inits2(precision, scale_x, scale_y, x_cur, y_cur, z_real,
            z_imag, mpfr_tmp1, mpfr_tmp2, z_abs_2, NULL);

    /* double scale_x = w / (double)view.w; */
    /* double scale_y = h / (double)view.h; */
    mpfr_div_d(scale_x, w, view.w, MPFR_RNDU);
    mpfr_div_d(scale_y, h, view.h, MPFR_RNDU);

    /* y_cur = y; */
    mpfr_set(y_cur, y, MPFR_RNDU);
    for (int py = view.y; py < view.y + view.h; py++) {
        /* x_cur = x; */
        mpfr_set(x_cur, x, MPFR_RNDU);
        for (int px = view.x; px < view.x + view.w; px++) {
            it = 0;  /* Iterations counter */
            /* z_real = x_cur; */
            mpfr_set(z_real, x_cur, MPFR_RNDU);
            /* z_imag = y_cur; */
            mpfr_set(z_imag, y_cur, MPFR_RNDU);
            // Calculate the square of the absolute value of z
            mpfr_sqr(mpfr_tmp1, z_real, MPFR_RNDU); /* pow(z_real, 2) */
            mpfr_sqr(mpfr_tmp2, z_imag, MPFR_RNDU); /* pow(z_imag, 2) */
            mpfr_add(z_abs_2, mpfr_tmp1, mpfr_tmp2, MPFR_RNDU); /* pow(z_real, 2) + pow(z_imag, 2) */
            while (mpfr_cmp_ui(z_abs_2, MY_INFINITY) <= 0 && it < max_iter) {
                it++;
                /* z = cpow(z, 2) + c
                 * z = (a + bI)(a + bI) + (d + eI)
                 * z = a^2 + 2*a*bI - b^2 + d + eI
                 * z_real = a^2 - b^2 + d
                 * z_imag = 2*a*b + e
                 * z_real = z_real^2 - z_imag^2 + x
                 * z_imag = 2*z_real*z_imag + y
                 */
                /* double a = pow(z_real, 2) - pow(z_imag, 2) + x_cur; */
                mpfr_sqr(mpfr_tmp1, z_real, MPFR_RNDU); /* pow(z_real, 2) */
                mpfr_sqr(mpfr_tmp2, z_imag, MPFR_RNDU); /* pow(z_imag, 2) */
                mpfr_sub(mpfr_tmp1, mpfr_tmp1, mpfr_tmp2, MPFR_RNDU); /* pow(z_real, 2) - pow(z_imag, 2) */
                mpfr_add(mpfr_tmp1, mpfr_tmp1, x_cur, MPFR_RNDU); /* pow(z_real, 2) - pow(z_imag, 2) + x_cur */
                /* double b = 2 * z_real * z_imag + y_cur; */
                mpfr_mul_ui(mpfr_tmp2, z_real, 2, MPFR_RNDU); /* 2 * z_real */
                mpfr_mul(mpfr_tmp2, mpfr_tmp2, z_imag, MPFR_RNDU); /* 2 * z_real * z_imag */
                mpfr_add(mpfr_tmp2, mpfr_tmp2, y_cur, MPFR_RNDU); /* 2 * z_real * z_imag + y_cur */
                /* z_real = a; */
                /* z_imag = b; */
                mpfr_set(z_real, mpfr_tmp1, MPFR_RNDU);
                mpfr_set(z_imag, mpfr_tmp2, MPFR_RNDU);

                // Calculate the square of the absolute value of z
                mpfr_sqr(mpfr_tmp1, z_real, MPFR_RNDU); /* pow(z_real, 2) */
                mpfr_sqr(mpfr_tmp2, z_imag, MPFR_RNDU); /* pow(z_imag, 2) */
                mpfr_add(z_abs_2, mpfr_tmp1, mpfr_tmp2, MPFR_RNDU); /* pow(z_real, 2) + pow(z_imag, 2) */
            }
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
            /* x_cur += scale_x; */
            mpfr_add(x_cur, x_cur, scale_x, MPFR_RNDU);
        }
        /* y_cur += scale_y; */
        mpfr_add(y_cur, y_cur, scale_y, MPFR_RNDU);
    }
    mpfr_clears(x, y, w, h, scale_x, scale_y, x_cur, y_cur, z_real, z_imag,
            mpfr_tmp1, mpfr_tmp2, z_abs_2, NULL);
}

struct render_rect_args {
    double x, y, w, h;
    mpfr_t x_hp, y_hp, w_hp, h_hp;
    SDL_Surface *img;
    SDL_Rect view;
    int max_iter;
    bool use_high_precision;
    long precision;
};

void *worker_render_rect(void *arguments)
{
    struct render_rect_args *args = arguments;
    if (args->use_high_precision) {
        render_rect_high_precision(args->x_hp, args->y_hp, args->w_hp,
                args->h_hp, args->img, args->view, args->max_iter,
                args->precision);
    } else {
        render_rect(args->x, args->y, args->w, args->h, args->img, args->view,
                args->max_iter);
    }
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

void enqueue_render(struct queue *q, struct viewport_mapping view, SDL_Surface *img, int max_iter, void *(*render_func)(void*), bool total)
{
//    if (view.use_high_precision)
//        printf("USING HIGH PRECISION!\n");
    if (view.view.w > 64) {
        SDL_Rect pix_a = {view.view.x, view.view.y, view.view.w/2, view.view.h};
        SDL_Rect pix_b = {view.view.x+view.view.w/2, view.view.y, view.view.w-view.view.w/2, view.view.h};
        struct viewport_mapping v1, v2;
        v1.use_high_precision = v2.use_high_precision = view.use_high_precision;
        v1.precision = v2.precision = view.precision;
        if (view.use_high_precision) {
//            mpfr_inits2(view.precision, v1.x_hp, v1.y_hp, v1.w_hp,
//                    v1.h_hp, v2.x_hp, v2.y_hp, v2.w_hp, v2.h_hp, NULL);
            mpfr_init2(v1.x_hp, v1.precision);
            mpfr_init2(v1.y_hp, v1.precision);
            mpfr_init2(v1.w_hp, v1.precision);
            mpfr_init2(v1.h_hp, v1.precision);
            mpfr_init2(v2.x_hp, v2.precision);
            mpfr_init2(v2.y_hp, v2.precision);
            mpfr_init2(v2.w_hp, v2.precision);
            mpfr_init2(v2.h_hp, v2.precision);
            mpfr_set(v1.x_hp, view.x_hp, MPFR_RNDN);
            mpfr_set(v1.y_hp, view.y_hp, MPFR_RNDN);
            mpfr_set(v1.h_hp, view.h_hp, MPFR_RNDN);
            mpfr_set(v2.y_hp, view.y_hp, MPFR_RNDN);
            mpfr_set(v2.h_hp, view.h_hp, MPFR_RNDN);
            mpfr_div_si(v1.w_hp, view.w_hp, 2, MPFR_RNDN);
            mpfr_set(v2.w_hp, v1.w_hp, MPFR_RNDN);
            mpfr_add(v2.x_hp, view.x_hp, v2.w_hp, MPFR_RNDN);
        } else {
            v1 = v2 = view;
            v1.w = view.w/2;
            v2.w = view.w/2;
            v2.x = view.x + v2.w;
        }
        v1.view = pix_a;
        v2.view = pix_b;
        enqueue_render(q, v1, img, max_iter, render_func, false);
        enqueue_render(q, v2, img, max_iter, render_func, false);
        return;
    }
    if (view.view.h > 36) {
        SDL_Rect pix_a = {view.view.x, view.view.y, view.view.w, view.view.h/2};
        SDL_Rect pix_b = {view.view.x, view.view.y+view.view.h/2, view.view.w, view.view.h-view.view.h/2};
        struct viewport_mapping v1, v2;
        v1.use_high_precision = v2.use_high_precision = view.use_high_precision;
        v1.precision = v2.precision = view.precision;
        if (view.use_high_precision) {
//            mpfr_inits2(view.precision, v1.x_hp, v1.y_hp, v1.w_hp,
//                    v1.h_hp, v2.x_hp, v2.y_hp, v2.w_hp, v2.h_hp, NULL);
            mpfr_init2(v1.x_hp, v1.precision);
            mpfr_init2(v1.y_hp, v1.precision);
            mpfr_init2(v1.w_hp, v1.precision);
            mpfr_init2(v1.h_hp, v1.precision);
            mpfr_init2(v2.x_hp, v2.precision);
            mpfr_init2(v2.y_hp, v2.precision);
            mpfr_init2(v2.w_hp, v2.precision);
            mpfr_init2(v2.h_hp, v2.precision);
            mpfr_set(v1.x_hp, view.x_hp, MPFR_RNDN);
            mpfr_set(v1.y_hp, view.y_hp, MPFR_RNDN);
            mpfr_set(v1.w_hp, view.w_hp, MPFR_RNDN);
            mpfr_set(v2.x_hp, view.x_hp, MPFR_RNDN);
            mpfr_set(v2.w_hp, view.w_hp, MPFR_RNDN);
            mpfr_div_si(v1.h_hp, view.h_hp, 2, MPFR_RNDN);
            mpfr_set(v2.h_hp, v1.h_hp, MPFR_RNDN);
            mpfr_add(v2.y_hp, view.y_hp, v2.h_hp, MPFR_RNDN);
        } else {
            v1 = v2 = view;
            v1.h = view.h/2;
            v2.h = view.h/2;
            v2.y = view.y + v2.h;
        }
        v1.view = pix_a;
        v2.view = pix_b;
        enqueue_render(q, v1, img, max_iter, render_func, false);
        enqueue_render(q, v2, img, max_iter, render_func, false);
        return;
    }
    struct render_rect_args *args = malloc(sizeof(struct render_rect_args));
    if (view.use_high_precision) {
        mpfr_init2(args->x_hp, view.precision);
        mpfr_init2(args->y_hp, view.precision);
        mpfr_init2(args->w_hp, view.precision);
        mpfr_init2(args->h_hp, view.precision);
        mpfr_set(args->x_hp, view.x_hp, MPFR_RNDN);
        mpfr_set(args->y_hp, view.y_hp, MPFR_RNDN);
        mpfr_set(args->w_hp, view.w_hp, MPFR_RNDN);
        mpfr_set(args->h_hp, view.h_hp, MPFR_RNDN);
    } else {
        args->x = view.x;
        args->y = view.y;
        args->w = view.w;
        args->h = view.h;
    }
    args->img = img;
    args->view = view.view;
    args->use_high_precision = view.use_high_precision;
    args->precision = view.precision;
    args->max_iter = max_iter;
    queue_add(q, render_func, args);
//    if (total)
//        queue_add(q, &signal_finished, img->userdata);
}

void draw(struct sdl_window_info win, struct viewport_mapping *view)
{
    struct viewport_mapping v = win.v;
    if (view != NULL) v = *view;
    enqueue_render(win.q, v, win.surf, win.max_iter, win.func, true);
}

void redraw(struct sdl_window_info win, struct viewport_mapping *view)
{
    if (view == NULL)
        sdl_blank_screen(win, win.v.view);
    else
        sdl_blank_screen(win, view->view);
//    SDL_UpdateWindowSurface(win.win)
    draw(win, view);
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
            y_max-y_min, IMG_WIDTH, IMG_HEIGHT, MAX_ITER, &worker_render_rect);

    printf("[MASTER   ] Creating worker threads...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    struct queue *task_queue = queue_init();
    window.q = task_queue;
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
    draw(window, NULL);
    clock_gettime(CLOCK_REALTIME, &end);
    printf("[MASTER   ] Created work queue in %.04lf seconds\n", nanos_diff(start, end)/(double)1000000000);

    long eventloop_i = 0;
    struct viewport_mapping redraw_area;
    while (window.keep_open) {
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
                            my_sdl_reset(&window);
                            redraw(window, NULL);
                            break;
                        case SDLK_i:
                        case SDLK_t:
                            viewport_zoom(&window, ZOOM_IN);
                            // TODO: Automatically switch to high-precision when width < 0.0000000000005
                            redraw(window, NULL);
                            break;
                        case SDLK_o:
                        case SDLK_g:
                            viewport_zoom(&window, ZOOM_OUT);
                            redraw(window, NULL);
                            break;
                        case SDLK_h:
                            viewport_mv(&window, MV_LEFT, &redraw_area);
                            redraw(window, &redraw_area);
                            break;
                        case SDLK_l:
                            viewport_mv(&window, MV_RIGHT, &redraw_area);
                            redraw(window, &redraw_area);
                            break;
                        case SDLK_k:
                            viewport_mv(&window, MV_UP, &redraw_area);
                            redraw(window, &redraw_area);
                            break;
                        case SDLK_j:
                            viewport_mv(&window, MV_DOWN, &redraw_area);
                            redraw(window, &redraw_area);
                            break;
                        case SDLK_UP:
                            if (window.max_iter <= 128)
                                window.max_iter *= 2;
                            else
                                window.max_iter += 128;
                            // TODO: Write iterations in a corner of the window
                            printf("[MASTER   ] Using %d iterations\n", window.max_iter);
                            redraw(window, NULL);
                            break;
                        case SDLK_DOWN:
                            if (window.max_iter == 1)
                                break;
                            if (window.max_iter <= 128)
                                window.max_iter /= 2;
                            else
                                window.max_iter -= 128;
                            printf("[MASTER   ] Using %d iterations\n", window.max_iter);
                            redraw(window, NULL);
                            break;
                        case SDLK_p:
                            toggle_high_precision(&window);
                            redraw(window, NULL);
                            break;
                    }
                    break;
            }
        }
        SDL_UpdateWindowSurface(window.win);
        SDL_Delay(33);
        eventloop_i++;
    }
    mpfr_free_cache();

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

//    save_png_to_file(&image, "out/image.png");
    return 0;
}
