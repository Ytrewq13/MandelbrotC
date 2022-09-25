#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <mpfr.h>

#include "tpool.h"

struct viewport_mapping {
    bool use_high_precision;
    long precision;
    double x, y, w, h;
    mpfr_t x_hp, y_hp, w_hp, h_hp;  /* High-precision floats */
    SDL_Rect view;
};

struct sdl_window_info {
    SDL_Window *win;
    SDL_Surface *surf;
    bool keep_open;
    bool _default_keep_open;
    struct viewport_mapping v;
    struct viewport_mapping _default_v;
    double mv_pct, zoom_pct;
    int max_iter;
    int _default_max_iter;
    void *(*func)(void *);
    void *(*_default_func)(void*);
    struct queue *q;
};

enum MV_DIR {
    MV_LEFT,
    MV_RIGHT,
    MV_UP,
    MV_DOWN,
};

enum ZOOM_DIR {
    ZOOM_IN,
    ZOOM_OUT,
};

struct sdl_window_info my_sdl_init(double x, double y, double w, double h,
        int w_w, int w_h, int max_iter, void *(*func)(void*));
void my_sdl_reset(struct sdl_window_info *win);
void sdl_blank_screen(struct sdl_window_info win, SDL_Rect blank_area);
void viewport_mv(struct sdl_window_info *win, enum MV_DIR dir, struct viewport_mapping *redraw_area);
void viewport_zoom(struct sdl_window_info *win, enum ZOOM_DIR dir);
void toggle_high_precision(struct sdl_window_info *win);
void enable_high_precision(struct sdl_window_info *win);
void disable_high_precision(struct sdl_window_info *win);
