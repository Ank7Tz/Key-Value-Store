LDLIBS=-lz -lpthread
CFLAGS=-ggdb3 -Wall -Wno-format-overflow -I.

EXES = dbserver dbtest

all: $(EXES)

dbtest: dbtest.o
	gcc $(CFLAGS) -o dbtest dbtest.o $(LDLIBS)

dbtest.o: dbtest.c
	gcc $(CFLAGS) -c dbtest.c -o dbtest.o

dbserver: dbserver.o workqueue.o record.o
	gcc $(CFLAGS) -o dbserver dbserver.o workqueue.o record.o $(LDLIBS)

dbserver.o: dbserver.c
	gcc $(CFLAGS) -c dbserver.c -o dbserver.o

workqueue.o: workqueue.c workqueue.h
	gcc $(CFLAGS) -c workqueue.c -o workqueue.o

record.o: record.c record.h
	gcc $(CFLAGS) -c record.c -o record.o

clean:
	rm -f $(EXES) *.o data.[0-9]*