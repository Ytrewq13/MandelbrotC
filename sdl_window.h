#include <SDL2/SDL.h>
#include <stdbool.h>

struct viewport_mapping {
    double x, y, w, h;
    SDL_Rect view;
};

struct sdl_window_info {
    SDL_Window *win;
    SDL_Surface *surf;
    bool keep_open;
    struct viewport_mapping v;
    int max_iter;
};

struct sdl_window_info my_sdl_init(double x, double y, double w, double h,
        int w_w, int w_h, int max_iter);
void sdl_blank_screen(struct sdl_window_info win);
