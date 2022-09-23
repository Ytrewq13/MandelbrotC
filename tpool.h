#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "png_maker.h"

struct queue {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    struct queue_item *first;
    struct queue_item *last;
};

struct queue_item {
    struct queue_item *prev;
    void *(*func)(void*);
    void *args;
    struct queue_item *next;
};

struct queue *queue_init(void);
void queue_destroy(struct queue *q);
void queue_add(struct queue *q, void *(*func)(void *), void *args);
void queue_get(struct queue *q, void (**func)(void *), void **args);
bool queue_empty(struct queue *q);


struct spin_thread_args {
    int id;
    struct queue *q;
    SDL_Surface *img_surf;
    bool *keep_window_open;
};

void *thread_spin(void*);
