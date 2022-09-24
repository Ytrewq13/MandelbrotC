#include "sdl_window.h"

struct sdl_window_info my_sdl_init(double x, double y, double w, double h,
        int w_w, int w_h, int max_iter, void *(*func)(void*))
{
    struct sdl_window_info ret;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "ERROR: Failed to initialise the SDL2 library\n");
        exit(EXIT_FAILURE);
    }
    ret.win = SDL_CreateWindow("SDL window", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, w_w, w_h, 0);
    if (ret.win == NULL) {
        fprintf(stderr, "ERROR: failed to create window\n");
        exit(EXIT_FAILURE);
    }
    ret.surf = SDL_GetWindowSurface(ret.win);
    if (ret.surf == NULL) {
        fprintf(stderr, "ERROR: failed to get surface from the window\n");
        exit(EXIT_FAILURE);
    }
    ret.keep_open = true;
    ret.v.view = (SDL_Rect) {.x=0, .y=0, .w=w_w, .h=w_h};
    ret.v.x = x;
    ret.v.y = y;
    ret.v.w = w;
    ret.v.h = h;
    ret.max_iter = max_iter;
    ret.func = func;

    ret._default_keep_open = ret.keep_open;
    ret._default_v = ret.v;
    ret._default_max_iter = ret.max_iter;
    ret._default_func = ret.func;
    return ret;
}

void my_sdl_reset(struct sdl_window_info *win)
{
    win->keep_open = win->_default_keep_open;
    win->v = win->_default_v;
    win->max_iter = win->_default_max_iter;
    win->func = win->_default_func;
}

void sdl_blank_screen(struct sdl_window_info win)
{
    SDL_Surface *new_surface = SDL_CreateRGBSurface(0, win.surf->w,
            win.surf->h, win.surf->format->BitsPerPixel, 0, 0, 0, 0);
    SDL_BlitSurface(new_surface, NULL, win.surf, NULL);
    SDL_FreeSurface(new_surface);
}
