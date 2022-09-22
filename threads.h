#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include "png_maker.h"

struct render_thread_info {
    int start;
    int end;
    bitmap_t *img;
};

struct queue {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    struct queue_item *first;
    struct queue_item *last;
};

struct queue_item {
    struct queue_item *prev;
    void *value;
    struct queue_item *next;
};

struct queue *queue_init(void);
void queue_destroy(struct queue *q);
void queue_add(struct queue *q, void *value);
void queue_get(struct queue *q, void **val_r);
bool queue_empty(struct queue *q);


struct spin_thread_args {
    int id;
    struct queue *q;
    bitmap_t *img;
};

void *thread_spin(void*);
