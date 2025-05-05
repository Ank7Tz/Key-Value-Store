#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include "request.h"
#include <errno.h>
#include "workqueue.h"
#include "record.h"
#include <ctype.h>

#define MAX_BACKLOG 2

void *listener(void *arg);
void *handle_work(void *arg);
void write_cmd(int fd, struct request *req);
void read_cmd(int fd, struct request *req);
void delete_cmd(int fd, struct request *req);
void quit_cmd();

int sockfd;

int fail_reqs;
int write_reqs;
int read_reqs;
int delete_reqs;

workqueue_t *workq;

pthread_mutex_t fail_req_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_req_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t read_req_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delete_req_mutex = PTHREAD_MUTEX_INITIALIZER;

void *listener(void *arg)
{
	struct addrinfo hints;
	struct addrinfo *res;
	int status;
	int recvfd;
	char *port;
	pthread_t worker1, worker2, worker3, worker4;

	port = (char *) arg;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, port, &hints, &res);

	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
		fprintf(stderr, "socket: %s\n", strerror(errno));
		exit(1);
	}

	if ((status = bind(sockfd, res->ai_addr, res->ai_addrlen)) < 0) {
		fprintf(stderr, "bind: %s\n", strerror(errno));
		exit(1);
	}

	if ((status = listen(sockfd, MAX_BACKLOG)) < 0) {
		fprintf(stderr, "listen: %s\n", strerror(errno));
		exit(1);
	}

	printf("listening on port: %s\n", port);

	workq = malloc(sizeof(workqueue_t));

	pthread_create(&worker1, NULL, handle_work, workq);
	pthread_create(&worker2, NULL, handle_work, workq);
	pthread_create(&worker3, NULL, handle_work, workq);
	pthread_create(&worker4, NULL, handle_work, workq);

	while (1) {
		if ((recvfd = accept(sockfd, NULL, NULL)) < 0) {
		       fprintf(stderr, "accept: %s\n", strerror(errno));
		}
		connection_t *con = malloc(sizeof(connection_t));
		con->fd = recvfd;

		queue_work(workq, con);
	}
}

void *handle_work(void *arg)
{
	struct request *req;
	int bytes_read;
	int recvfd;

	workqueue_t *queue = (workqueue_t *)arg;

	while (1) {
		connection_t *con = get_work(queue);

		recvfd = con->fd;

		req = (struct request *) malloc(sizeof(struct request));

		bytes_read = recv(recvfd, req, sizeof(struct request), 0);
		if (bytes_read != sizeof(struct request)) {
			perror("read_request: request size doesn't match\n");
			close(recvfd);
			continue;
		}
	
		if (req->op_status == 'W') {
			write_cmd(recvfd, req);
		} else if (req->op_status == 'R') {
			read_cmd(recvfd, req);
		} else if (req->op_status == 'D') {
			delete_cmd(recvfd, req);
		} else if (req->op_status == 'Q') {
			quit_cmd();
		} else {
			perror("invalid op\n");
			fail_reqs++;
		}

		close(recvfd);
		free(con);
	}

	return NULL;
}

void read_cmd(int fd, struct request *req)
{
	char data[4096];
	int bytes;
	struct request res;
	
	pthread_mutex_lock(&read_req_mutex);
	read_reqs++;
	pthread_mutex_unlock(&read_req_mutex);

	bytes = read_key(req->name, data, sizeof(data));
	strncpy(res.name, req->name, sizeof(req->name));

	if (bytes < 0) {
		pthread_mutex_lock(&fail_req_mutex);
		fail_reqs++;
		pthread_mutex_unlock(&fail_req_mutex);

		res.op_status = 'X';
		if (send(fd, &res, sizeof(res), 0) < 0) {
			perror("read_cmd: error sending response when bytes < 0\n");
			return;
		}
	} else {
		res.op_status = 'K';
		sprintf(res.len, "%d", bytes);

		if (send(fd, &res, sizeof(res), 0) < 0) {
			perror("read_cmd: error sending response headers\n");
			return;
		}

		if (send(fd, data, bytes, 0) < 0) {
			perror("read_cmd: error sending response payload\n");
			return;
		}
		//printf("successful read response sent!\n");
	}
}

void quit_cmd()
{
	printf("quitting the server!\n");
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	exit(0);
}

void write_cmd(int fd, struct request *req)
{
	char data[4096];
	struct request res;

	pthread_mutex_lock(&write_req_mutex);
	write_reqs++;
	pthread_mutex_unlock(&write_req_mutex);

	int len = atoi(req->len);

	int bytes = recv(fd, data, len, 0);

	if (bytes != len) {
		perror("bytes != len\n");
		close(fd);
	}

	int r = write_key(req->name, data, len);

	if (r < 0) {
		pthread_mutex_lock(&fail_req_mutex);
		fail_reqs++;
		pthread_mutex_unlock(&fail_req_mutex);

		res.op_status = 'X';
	} else {
		res.op_status = 'K';
	}

	strncpy(res.name, req->name, sizeof(req->name));

	if (send(fd, &res, sizeof(res), 0) < 0) {
		perror("write_cmd: error sending response!\n");
		return;
	}
	//printf("write response sent!\n");
}

void delete_cmd(int fd, struct request *req)
{
	struct request res;

	pthread_mutex_lock(&delete_req_mutex);
	delete_reqs++;
	pthread_mutex_unlock(&delete_req_mutex);

	int r = delete_key(req->name);

	strncpy(res.name, req->name, sizeof(req->name));

	if (r < 0) {
		pthread_mutex_lock(&fail_req_mutex);
		fail_reqs++;
		pthread_mutex_unlock(&fail_req_mutex);

		res.op_status = 'X';
	} else {
		res.op_status = 'K';
	}

	if (send(fd, &res, sizeof(res), 0) < 0) {
		perror("delete_cmd: error sending response!\n");
		return;
	}
	//printf("delete response sent!\n");
}

int isnumber(char *num)
{
	if (num == NULL || *num == '\0')
		return 0;

	int i = 0;

	if (num[0] == '+' || num[0] == '-')
		i = 1;

	for (; num[i] != '\0'; i++)
		if (!isdigit(num[i]))
			return 0;

	return 1;
}

int main(int argc, char *argv[])
{
	pthread_t listener_thread;
	char *port;
	char input[128];
	char word[8];

	if (argc == 1) {
		port = "5000";
	} else if (argc == 2) {
		if (!isnumber(argv[1])) {
			fprintf(stderr, "port needs to be number");
			return 1;
		}

		port = argv[1];
	} else {
		fprintf(stderr, "usage: ./dbserver {port}\n");
		return 1;
	}


	pthread_create(&listener_thread, NULL, listener, port);

	while (fgets(input, sizeof(input), stdin) != NULL) {
		sscanf(input, "%7s", word);

		if (strcmp(word, "stats") == 0) {
			int total_objects = count_objects();
			int queue_size = workqueue_size(workq);
			fprintf(stdout, "Total Objects = %d\tTotal Reads = %d\tTotal Writes = %d\tTotal Deletes = %d\tTotal Failed = %d\tRequest Queue = %d\n", total_objects, read_reqs, write_reqs, delete_reqs, fail_reqs, queue_size);
		} else if (strcmp(word, "quit") == 0) {
			quit_cmd();
		} else {
			fprintf(stdout, "invalid command\n");
		}
		memset(word, 0, sizeof(word));
	}

	pthread_join(listener_thread, NULL);

	return 0;
}