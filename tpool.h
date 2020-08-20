#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define ERROR(Str) fprintf(stderr,"%s\n",Str), exit(1)
#define NUM_THREADS 5

struct tpool;

typedef struct tpool tpool_t;

typedef struct tpool_work tpool_work_t;

struct tpool {
	tpool_work_t *work_first; //push work object
	tpool_work_t *work_last; //pop work object
	pthread_mutex_t work_mutex; //used for locking
	pthread_cond_t work_cond; //signals the threads that there is work to be procesed
	pthread_cond_t working_cond; //signals when there are no threads processing
	size_t working_cnt; //how many threads are actively processing work
	size_t thread_cnt; //tracks how many threads are alive
	bool stop; //stops threads
};
typedef void *(*thread_func_t)(void *arg);

struct tpool_work { //linked list which stores the function to call and its arguments
	thread_func_t func;
	void *arg;
	struct tpool_work *next;
};

typedef void(*func)(void *x);

typedef struct my_function{
	void * result;
	void * func;
} func;

typedef struct function_node_t{ //our function node
	func function;
	struct function_node_t *next;
} function_node_t;

typedef struct function_queue_t{ //our queue of functions
	function_node_t *head;
	function_node_t *tail;
	int size;
} function_queue_t;

typedef struct thread_manager{
	pthread_t * many_to_one;
	tpool_t pool;
	function_queue_t * queue;
/* Mutex */
/* Timer */
	int mode;
} thread_manager;
::;
pthread_t * thread_dequeue(thread_queue_t *q);


tpool_t *tpool_create(size_t num);
void tpool_destroy(tpool_t *tm);

bool tpool_add_work(tpool_t *tm,thread_func_t func, void *arg);
void tpool_wait(tpool_t *tm);

#endif
