#include <SDL2/SDL.h>
#include <stdbool.h>

#include "tpool.h"

struct viewport_mapping {
    double x, y, w, h;
    SDL_Rect view;
};

struct sdl_window_info {
    SDL_Window *win;
    SDL_Surface *surf;
    bool keep_open;
    bool _default_keep_open;
    struct viewport_mapping v;
    struct viewport_mapping _default_v;
    int max_iter;
    int _default_max_iter;
    void *(*func)(void *);
    void *(*_default_func)(void*);
    struct queue *q;
};

struct sdl_window_info my_sdl_init(double x, double y, double w, double h,
        int w_w, int w_h, int max_iter, void *(*func)(void*));
void my_sdl_reset(struct sdl_window_info *win);
void sdl_blank_screen(struct sdl_window_info win);
