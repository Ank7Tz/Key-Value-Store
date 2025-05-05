#include "record.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define BASEFILE "/tmp/data.%d"
#define N 200

pthread_mutex_t mutexes[N] = { PTHREAD_MUTEX_INITIALIZER };
pthread_cond_t conds[N] = { PTHREAD_COND_INITIALIZER };

record_t table[N];

void make_key_valid(int i)
{
	pthread_mutex_lock(&mutexes[i]);
	table[i].state = 1;
	pthread_cond_broadcast(&conds[i]);
	pthread_mutex_unlock(&mutexes[i]);
}

int find_key(char *key, int iswrite)
{
	int index = -1;

	for (int i = 0; i < N; i++) {
		pthread_mutex_lock(&mutexes[i]);
		if (strcmp(table[i].key, key) == 0) {
			while (table[i].state == 2)
				pthread_cond_wait(&conds[i], &mutexes[i]);

			if (!iswrite && (table[i].state == 0 || strcmp(table[i].key, key) != 0)) {
				pthread_mutex_unlock(&mutexes[i]);
				break;
			}

			table[i].state = 2;
			index = i;
			pthread_mutex_unlock(&mutexes[i]);
			break;
		}
		pthread_mutex_unlock(&mutexes[i]);
	}

	return index;
}



int read_key(char *key, char *buffer, int len)
{
	int index;
	if ((index = find_key(key, 0)) < 0)
		return -1;

	char filename[50];
	int fd;

	sprintf(filename, BASEFILE, index);

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("read_key: can't open\n");
		make_key_valid(index);
		return -1;
	}

	usleep(random() % 10000);

	int size = read(fd, buffer, len);

	close(fd);


	make_key_valid(index);

	return size;
}

int write_key(char *key, char *buffer, int len)
{
	int index = -1;
	if ((index = find_key(key, 1)) < 0) {
		for (int i = 0; i < N; i++) {
			pthread_mutex_lock(&mutexes[i]);
			if (table[i].state == 0) {
				index = i;
				strncpy(table[i].key, key, sizeof(table[i].key) - 1);

				table[i].state = 2;
				pthread_mutex_unlock(&mutexes[i]);
				break;
			}
			pthread_mutex_unlock(&mutexes[i]);
		}
	} 


	if (index < 0) {
		return -1;
	}

	char filename[50];
	int fd;
	
	sprintf(filename, BASEFILE, index);

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0) {
		perror("write_key: can't open\n");
		return -1;
	}

	usleep(random() % 10000);

	int size = write(fd, buffer, len);

	if (size < 0) {
		perror("write_key: error writing to file!\n");
		return -1;
	}

	close(fd);

	make_key_valid(index);

	return size;
}

int delete_key(char *key)
{
	int index = -1;
	char filename[50];
	for (int i = 0; i < N; i++) {
		pthread_mutex_lock(&mutexes[i]);
		if (strcmp(table[i].key, key) == 0) {
			while (table[i].state == 2)
				pthread_cond_wait(&conds[i], &mutexes[i]);

			if (table[i].state == 0) {
				pthread_mutex_unlock(&mutexes[i]);
				break;
			}

			sprintf(filename, BASEFILE, i);

			if (unlink(filename) != 0) {
				perror("delete_key: unable to delete the file!\n");
				pthread_mutex_unlock(&mutexes[i]);
				break;
			}

			index = i;
			table[i].state = 0;
			strcpy(table[i].key, "");
			pthread_cond_broadcast(&conds[i]);
			pthread_mutex_unlock(&mutexes[i]);
			break;
		}
		pthread_mutex_unlock(&mutexes[i]);
	}

	return index;
}

int count_objects()
{
	int count = 0;
	for (int i = 0; i < N; i++) {
		if (table[i].state != 0) {
			count++;
		}
	}

	return count;
}