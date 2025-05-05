#include "workqueue.h"
#include <stdlib.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void queue_work(workqueue_t *queue, connection_t *element)
{
	node_t *new_node = (node_t *) malloc(sizeof(node_t));

	new_node->data = element;
	new_node->next = NULL;

	pthread_mutex_lock(&mutex);

	if (queue->size == 0)
		queue->head = new_node;
	else
		queue->tail->next = new_node;
	queue->tail = new_node;
	queue->size++;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
}

connection_t *get_work(workqueue_t *queue)
{
	pthread_mutex_lock(&mutex);
	while (queue->size == 0)
		pthread_cond_wait(&cond, &mutex);

	node_t *temp = queue->head;
	queue->head = queue->head->next;
	queue->size--;

	if (queue->head == NULL)
		queue->tail = NULL;

	connection_t *data = temp->data;

	free(temp);

	pthread_mutex_unlock(&mutex);

	return data;
}

int workqueue_size(workqueue_t *queue)
{
	int count;
	pthread_mutex_lock(&mutex);
	count = queue->size;
	pthread_mutex_unlock(&mutex);
	return count;
}