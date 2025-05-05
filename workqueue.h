#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__

#include <pthread.h>

typedef struct connection_t {
	int fd;
} connection_t;

typedef struct node_t {
	connection_t *data;
	struct node_t *next;
} node_t;

typedef struct workqueue {
	node_t *head;
	node_t *tail;
	int size;
} workqueue_t;

void queue_work(workqueue_t *queue, connection_t *element);

connection_t *get_work(workqueue_t *queue);

int workqueue_size(workqueue_t *queue);

#endif