#include "tpool.h"

bool queue_empty(struct queue *q)
{ return (q == NULL) || (q->first == NULL) || (q->last == NULL); }

struct queue *queue_init()
{
    struct queue *q = (struct queue *)malloc(sizeof(struct queue));
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->first = NULL;
    q->last = NULL;
    return q;
}

void queue_destroy(struct queue *q)
{
    /* Empty the queue */
    struct queue_item *cur, *next;
    if (!queue_empty(q)) {
        cur = q->first;
        while (cur != NULL) {
            next = cur->next;
            free(cur);
            cur = next;
        }
    }
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cond);
    free(q);
}

void queue_add(struct queue * q, void *(*func)(void *), void *args)
{
    struct queue_item *new;
    new = (struct queue_item*)malloc(sizeof(struct queue_item));
    new->prev = NULL;
    new->next = NULL;
    new->func = func;
    new->args = args;

    pthread_mutex_lock(&q->mtx);

    if (q && q->first && q->last) {
        q->last->next = new;
        new->prev = q->last;
        q->last = new;
    } else {
        q->first = new;
        q->last = new;
    }

    pthread_mutex_unlock(&q->mtx);

    /* Signal waiting threads */
    pthread_cond_signal(&q->cond);
}

void queue_get(struct queue *q, void (**func)(void *), void **args)
{
    pthread_mutex_lock(&q->mtx);

    while (queue_empty(q))
        pthread_cond_wait(&q->cond, &q->mtx);

    /* Get the item from the first queue entry and free that entry */
    *func = q->first->func;
    *args = q->first->args;
    struct queue_item *next = q->first->next;
    free(q->first);
    q->first = next;

    pthread_mutex_unlock(&q->mtx);
}
