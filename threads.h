#include <pthread.h>
#include "png_maker.h"

struct render_thread_info {
    int start;
    int end;
    bitmap_t *img;
};

