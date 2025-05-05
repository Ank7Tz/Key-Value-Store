#ifndef __RECORDTABLE_H__
#define __RECORDTABLE_H__

typedef struct record_t {
	char state; /*0 = invalid, 1 = valid, 2 = busy*/
	char key[31];
} record_t;

int read_key(char *key, char *buffer, int len);
int write_key(char *key, char *buffer, int len);
int delete_key(char *key);
int count_objects();
#endif