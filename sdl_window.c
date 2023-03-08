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
    ret.v.use_high_precision = false;
    ret.v.precision = 200;
//    mpfr_set_default_prec(ret.v.precision);
    ret.v.x = x;
    ret.v.y = y;
    ret.v.w = w;
    ret.v.h = h;
    ret.mv_pct = 0.1;
    ret.zoom_pct = 0.6;
    /* Initialise the high-precision values */
//    mpfr_init2(ret.v.x_hp, 200);
//    mpfr_init2(ret.v.y_hp, 200);
//    mpfr_init2(ret.v.w_hp, 200);
//    mpfr_init2(ret.v.h_hp, 200);
//    mpfr_set_d(ret.v.x_hp, x, MPFR_RNDN);
//    mpfr_set_d(ret.v.y_hp, y, MPFR_RNDN);
//    mpfr_set_d(ret.v.w_hp, w, MPFR_RNDN);
//    mpfr_set_d(ret.v.h_hp, h, MPFR_RNDN);
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
    /* Restore high-precision values */
//    mpfr_set_d(win->v.x_hp, win->v.x, MPFR_RNDN);
//    mpfr_set_d(win->v.y_hp, win->v.y, MPFR_RNDN);
//    mpfr_set_d(win->v.w_hp, win->v.w, MPFR_RNDN);
//    mpfr_set_d(win->v.h_hp, win->v.h, MPFR_RNDN);
    win->max_iter = win->_default_max_iter;
    win->func = win->_default_func;
}

void sdl_blank_screen(struct sdl_window_info win, SDL_Rect blank_area)
{
    SDL_Surface *new_surface = SDL_CreateRGBSurface(0, blank_area.w,
            blank_area.h, win.surf->format->BitsPerPixel, 0, 0, 0, 0);
    SDL_BlitSurface(new_surface, NULL, win.surf, &blank_area);
    SDL_FreeSurface(new_surface);
}

/* Move the viewport in a given direction.
 * Takes into account the mv_pct as the fraction of the viewport width/height
 * to move by.
 * Supports high_precision MPFR numbers. */
void viewport_mv(struct sdl_window_info *win, enum MV_DIR dir, struct viewport_mapping *redraw_area)
{
    mpfr_t dx_hp, dy_hp;
    double dx = 0, dy = 0;
    bool high_precision = win->v.use_high_precision;
    SDL_Rect keep_area, dest_area, discard_area;
    if (high_precision)
        mpfr_inits2(win->v.precision, dx_hp, dy_hp, NULL);
    switch (dir) {
        case MV_LEFT:
                keep_area.w = win->v.view.w * (1 - win->mv_pct);
                keep_area.h = win->v.view.h;
                keep_area.x = 0;
                keep_area.y = 0;
                dest_area.w = keep_area.w;
                dest_area.h = keep_area.h;
                dest_area.x = win->v.view.w - keep_area.w;
                dest_area.y = 0;
                discard_area.w = win->v.view.w - keep_area.w;
                discard_area.h = win->v.view.h;
                discard_area.x = 0;
                discard_area.y = 0;
            if (high_precision) {
                mpfr_mul_d(dx_hp, win->v.w_hp, -1*win->mv_pct, MPFR_RNDN);
                mpfr_set_d(dy_hp, 0, MPFR_RNDZ);
            } else {
                dx = -1 * win->v.w * win->mv_pct;
            }
            break;
        case MV_RIGHT:
                keep_area.w = win->v.view.w * (1 - win->mv_pct);
                keep_area.h = win->v.view.h;
                keep_area.x = win->v.view.w - keep_area.w;
                keep_area.y = 0;
                dest_area.w = keep_area.w;
                dest_area.h = keep_area.h;
                dest_area.x = 0;
                dest_area.y = 0;
                discard_area.w = win->v.view.w - keep_area.w;
                discard_area.h = win->v.view.h;
                discard_area.x = keep_area.w;
                discard_area.y = 0;
            if (high_precision) {
                mpfr_mul_d(dx_hp, win->v.w_hp, win->mv_pct, MPFR_RNDN);
                mpfr_set_d(dy_hp, 0, MPFR_RNDZ);
            } else {
                dx = win->v.w * win->mv_pct;
            }
            break;
        case MV_DOWN:
                keep_area.w = win->v.view.w;
                keep_area.h = win->v.view.h * (1 - win->mv_pct);;
                keep_area.x = 0;
                keep_area.y = win->v.view.h - keep_area.h;
                dest_area.w = keep_area.w;
                dest_area.h = keep_area.h;
                dest_area.x = 0;
                dest_area.y = 0;
                discard_area.w = win->v.view.w;
                discard_area.h = win->v.view.h - keep_area.h;
                discard_area.x = 0;
                discard_area.y = keep_area.h;
            if (high_precision) {
                mpfr_set_d(dx_hp, 0, MPFR_RNDZ);
                mpfr_mul_d(dy_hp, win->v.h_hp, win->mv_pct, MPFR_RNDN);
            } else {
                dy = win->v.h * win->mv_pct;
            }
            break;
        case MV_UP:
        default:
                keep_area.w = win->v.view.w;
                keep_area.h = win->v.view.h * (1 - win->mv_pct);;
                keep_area.x = 0;
                keep_area.y = 0;
                dest_area.w = keep_area.w;
                dest_area.h = keep_area.h;
                dest_area.x = 0;
                dest_area.y = win->v.view.h - keep_area.h;
                discard_area.w = win->v.view.w;
                discard_area.h = win->v.view.h - keep_area.h;
                discard_area.x = 0;
                discard_area.y = 0;
            if (high_precision) {
                mpfr_set_d(dx_hp, 0, MPFR_RNDZ);
                mpfr_mul_d(dy_hp, win->v.h_hp, -1*win->mv_pct, MPFR_RNDN);
            } else {
                dy = -1 * win->v.h * win->mv_pct;
            }
            break;
    }
    if (high_precision) {
        mpfr_add(win->v.x_hp, win->v.x_hp, dx_hp, MPFR_RNDN);
        mpfr_add(win->v.y_hp, win->v.y_hp, dy_hp, MPFR_RNDN);
        mpfr_clears(dx_hp, dy_hp, NULL);
    } else {
        win->v.x += dx;
        win->v.y += dy;
    }
    if (redraw_area != NULL) {
        redraw_area->view = discard_area;
    }
    // Try a direct copy:
    SDL_BlitSurface(win->surf, &keep_area, win->surf, &dest_area);
    // If that doesn't work do it indirectly:
//    SDL_Surface *temp_surf = SDL_CreateRGBSurface(0, keep_area.w,
//            keep_area.h, win->surf->format->BitsPerPixel, 0, 0, 0, 0);
//    SDL_BlitSurface(win->surf, &keep_area, temp_surf, NULL);
//    SDL_BlitSurface(temp_surf, NULL, win->surf, &dest_area);
//    SDL_FreeSurface(temp_surf);
    if (redraw_area != NULL) {
        *redraw_area = win->v;
        redraw_area->view = discard_area;
        if (win->v.use_high_precision) {
            mpfr_inits2(win->v.precision, redraw_area->w_hp, redraw_area->h_hp,
                    redraw_area->x_hp, redraw_area->y_hp, NULL);
            mpfr_mul_d(redraw_area->w_hp, win->v.w_hp, redraw_area->view.w /
                    (double) win->v.view.w, MPFR_RNDN);
            mpfr_mul_d(redraw_area->h_hp, win->v.h_hp, redraw_area->view.h /
                    (double) win->v.view.h, MPFR_RNDN);
            mpfr_mul_d(redraw_area->x_hp, win->v.w_hp, discard_area.x / (double)
                win->v.view.w, MPFR_RNDN);
            mpfr_add(redraw_area->x_hp, redraw_area->x_hp, win->v.x_hp, MPFR_RNDN);
            mpfr_mul_d(redraw_area->y_hp, win->v.h_hp, discard_area.y / (double)
                win->v.view.h, MPFR_RNDN);
            mpfr_add(redraw_area->y_hp, redraw_area->y_hp, win->v.y_hp, MPFR_RNDN);
        } else {
            redraw_area->w = win->v.w * redraw_area->view.w /
                (double) win->v.view.w;
            redraw_area->h = win->v.h * redraw_area->view.h /
                (double) win->v.view.h;
            redraw_area->x = win->v.x + win->v.w * (discard_area.x / (double)
                win->v.view.w);
            redraw_area->y = win->v.y + win->v.h * (discard_area.y / (double)
                win->v.view.h);
        }
    }
}

void viewport_zoom(struct sdl_window_info *win, enum ZOOM_DIR dir)
{
    double cx, cy, scale;
    bool high_precision = win->v.use_high_precision;
    switch (dir) {
        case ZOOM_IN:
            /* Zoom in: w,h decrease (multiply by smaller number) */
            scale = 1 - win->zoom_pct;
            break;
        case ZOOM_OUT:
        default:
            scale = 1.0 / (1 - win->zoom_pct);
            break;
    }
    /* ZOOM IN:
     * x0 = 9
     * w0 = 2
     * scale = 0.8 = 1 - 0.2 (zoom_pct = 0.2)
     * center = 10
     * w1 = 1.6
     * x1 = 9.2
     * ZOOM OUT:
     * x0 = 9.2
     * w0 = 1.6
     * scale = 1.25
     * center = 10
     * w1 = 2
     * x1 = 10 - 2/2 = 9
     */
    if (high_precision) {
        mpfr_t tmp_hp, cx_hp, cy_hp;
        mpfr_inits2(mpfr_get_prec(win->v.x_hp), tmp_hp, cx_hp, cy_hp, NULL);
        /* Get the center position */
        mpfr_div_d(tmp_hp, win->v.w_hp, 2.0, MPFR_RNDN);
        mpfr_add(cx_hp, win->v.x_hp, tmp_hp, MPFR_RNDN);
        mpfr_div_d(tmp_hp, win->v.h_hp, 2.0, MPFR_RNDN);
        mpfr_add(cy_hp, win->v.y_hp, tmp_hp, MPFR_RNDN);
        /* Scale the width and height */
        mpfr_mul_d(win->v.w_hp, win->v.w_hp, scale, MPFR_RNDN);
        mpfr_mul_d(win->v.h_hp, win->v.h_hp, scale, MPFR_RNDN);
        /* Re-calculate the x and y of the top corner */
        mpfr_div_d(tmp_hp, win->v.w_hp, 2.0, MPFR_RNDN);
        mpfr_sub(win->v.x_hp, cx_hp, tmp_hp, MPFR_RNDN);
        mpfr_div_d(tmp_hp, win->v.h_hp, 2.0, MPFR_RNDN);
        mpfr_sub(win->v.y_hp, cy_hp, tmp_hp, MPFR_RNDN);
        /* Free the temporary mpfr_t variables */
        mpfr_clears(tmp_hp, cx_hp, cy_hp, NULL);
    } else {
        /* Get the center position */
        cx = win->v.x + win->v.w/2.0;
        cy = win->v.y + win->v.h/2.0;
        /* Scale the width and height */
        win->v.w *= scale;
        win->v.h *= scale;
        /* Re-calculate the x and y of the top corner */
        win->v.x = cx - win->v.w/2.0;
        win->v.y = cy - win->v.h/2.0;
    }
}

void toggle_high_precision(struct sdl_window_info *win)
{
    if (win->v.use_high_precision)
        disable_high_precision(win);
    else
        enable_high_precision(win);
}

void enable_high_precision(struct sdl_window_info *win)
{
    win->v.use_high_precision = true;
    mpfr_inits2(win->v.precision, win->v.x_hp, win->v.y_hp, win->v.w_hp, win->v.h_hp, NULL);
    mpfr_set_d(win->v.x_hp, win->v.x, MPFR_RNDN);
    mpfr_set_d(win->v.y_hp, win->v.y, MPFR_RNDN);
    mpfr_set_d(win->v.w_hp, win->v.w, MPFR_RNDN);
    mpfr_set_d(win->v.h_hp, win->v.h, MPFR_RNDN);
}

void disable_high_precision(struct sdl_window_info *win)
{
    win->v.use_high_precision = false;
    win->v.x = mpfr_get_d(win->v.x_hp, MPFR_RNDN);
    win->v.y = mpfr_get_d(win->v.y_hp, MPFR_RNDN);
    win->v.w = mpfr_get_d(win->v.w_hp, MPFR_RNDN);
    win->v.h = mpfr_get_d(win->v.h_hp, MPFR_RNDN);
    mpfr_clears(win->v.x_hp, win->v.y_hp, win->v.w_hp, win->v.h_hp, NULL);
}
