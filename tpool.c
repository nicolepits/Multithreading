#include "tpool.h"

/* POSIX Timers */
#include <signal.h>
#include <time.h>

#define SIG SIGRTMIN
#define CLOCKID CLOCK_REALTIME
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
		       } while (0)


void function_enqueue(func my_function,function_queue_t *q){
	function_node_t *node = malloc(sizeof(*node)); //allocate node
	node->function = function;

	if(q->size == 0){ //check if queue is empty
		q->head = node;
		q->tail = node;
	} else {
		q->tail->next = node;
		q->tail = node;
	}
	q->size++;
}

function function_dequeue(function_queue_t *q){
	if(q->size==0){
		ERROR("Queue is empty, cannot dequeue thread");
	}
	function_node_t *newHead = malloc(sizeof(*newHead)); //grab next element after head
	newHead = q->head->next; 

	function_node_t *oldHead = malloc(sizeof(*oldHead)); // remove the head
	oldHead = q->head;

	if(q->size==1){
		q->tail=NULL;
		q->size=NULL;
		return oldHead->function;
	}

	//set a new head
	q->head = newHead;
	q->size--;
	return oldHead->function;
}


static tpool_work_t *tpool_work_create(thread_func_t func, void *arg){
	
	tpool_work_t *work;

	if(func == NULL)
		return NULL;

	work = malloc(sizeof(*work));
	work->func = func;
	work->arg = arg;
	work->next = NULL;
	return work;
}

static void tpool_work_destroy(tpool_work_t *work){

	if(work==NULL)
		return;
	free(work);
}


static tpool_work_t *tpool_work_get(tpool_t *tm){

	tpool_work_t *work;

	if(tm==NULL)
		return NULL;
	work=tm->work_first;
	
	if(work==NULL)
		return NULL;
	if(work->next==NULL){
		tm->work_first=NULL;
		tm->work_last=NULL;
	} else {
		tm->work_first= work->next;
	}
	return work;
}

static void *tpool_worker(void *arg){

	tpool_t *tm = arg;
	tpool_work_t *work;

	while(1){
		pthread_mutex_lock(&(tm->work_mutex)); //locks the mutex so that nothing else manipulates the pool's members

		while(tm->work_first == NULL && !tm->stop) //check if there is any work available for processing until we are signaled and run out check again
			pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex)); 
		if(tm->stop) //check if the pool has requested that all threads stop running and exit.
		break;

		work = tpool_work_get(tm); //pull work from queue
		tm->working_cnt++; //increment so the pool knows a thread is processing
		pthread_mutex_unlock(&(tm->work_mutex)); //unlock the mutex so the other threads can pull and process work 

		if(work!=NULL){ //if there was work, process it and destroy the work object
			//(work->func)(work->arg);
			tpool_work_destroy(work);
		}

		pthread_mutex_lock(&(tm->work_mutex)); //once the work has been processed , we lock the mutex 
		tm->working_cnt--; //decrease the working counter because the work is done
		if(!tm->stop && tm->working_cnt ==0 && tm-pthread_cond_signal(&(tm->working_cond)))
				pthread_mutex_unlock(&(tm->work_mutex)); //if there are no threads working and three are no items in queue, a signal will be sent to inform wait function to wake up
	}

	tm->thread_cnt--; //the thread is stopping
	pthread_cond_signal(&(tm->working_cond)); //we signal tpool_wait
	pthread_mutex_unlock(&(tm->work_mutex)); //unlock mutex because everything is protected by it
	return NULL;

}

tpool_t *pool_create(size_t num){
	tpool_t *tm;
	pthread_t thread;
	size_t i;
	pthread_attr_t attr;
	//set scope
	pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);

	if(num == 0)
		num=2;
	tm = calloc(1,sizeof(*tm));
	tm->thread_cnt = num;

	pthread_mutex_init(&(tm->work_mutex),NULL);
	pthread_cond_init(&(tm->work_cond),NULL);
	pthread_cond_init(&(tm->working_cond),NULL);

	tm->work_first=NULL;
	tm->work_last=NULL;

	for(i=0;i<num;i++){
		pthread_create(&thread,&attr,tpool_worker,tm);
		pthread_detach(thread);
	}

	return tm;
}

void tpool_destroy(tpool_t *tm){
	tpool_work_t *work;
	tpool_work_t *work2;

	if(tm==NULL)
		return;
	pthread_mutex_lock(&(tm->work_mutex));
	work=tm->work_first;
	while(work!=NULL){
		work2 = work->next;
		tpool_work_destroy(work);
		work=work2;
	}
	tm->stop=true;
	pthread_cond_broadcast(&(tm->work_cond));
	pthread_mutex_unlock(&(tm->work_mutex));

	tpool_wait(tm);

	pthread_mutex_destroy(&(tm->work_mutex));
	pthread_cond_destroy(&(tm->work_cond));
	pthread_cond_destroy(&(tm->working_cond));

	free(tm);

}

bool tpool_add_work(tpool_t *tm,thread_func_t func,void *arg){
	tpool_work_t *work;

	if(tm==NULL)
		return false;
	work = tpool_work_create(func,arg);
	if(work==NULL)
		return false;

	pthread_mutex_lock(&(tm->work_mutex));
	if(tm->work_first==NULL){
		tm->work_first=work;
		tm->work_last=tm->work_first;
	} else {
		tm->work_last->next = work;
		tm->work_last = work;
	}
	
	pthread_cond_broadcast(&(tm->work_cond));
	pthread_mutex_unlock(&(tm->work_mutex));

	return true;
}

void tpool_wait(tpool_t *tm){
	if(tm==NULL)
		return;
	pthread_mutex_lock(&(tm->work_mutex));

	while(1){
		if((!tm->stop && tm->working_cnt!=0)||(tm->stop && tm->thread_cnt!=0)){
			pthread_cond_wait(&(tm->working_cond),&(tm->work_mutex));
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&(tm->work_mutex));

}

int one_to_one(void *func(void*),void *arg){

	int rc;
	pthread_t thread[1];

	pthread_attr_t attr;
	pthread_attr_setscope(&attr,PTHREAD_SCOPE_PROCESS);
	
	rc=pthread_create(&thread[0],&attr,func,arg);

	if(rc){
		printf("ERROR; Return code from pthread_create() is %d\n",rc);
		exit(1);
	}
	pthread_exit(NULL);
}

static void * many_to_one_worker(void *arg);
thread_manager many_to_one(){

	int rc;
	thread_manager mgr = { 0 };


	rc=pthread_create(&mgr.many_to_one, NULL, &many_to_one_worker, &mgr);
	printf("pthread_create ret = %d\n", rc);
/*
- initialize mutex

*/
	return mgr;
}


       static void
       handler(int sig, siginfo_t *si, void *uc)
       {
           /* Note: calling printf() from a signal handler is not safe
              (and should not be done in production programs), since
              printf() is not async-signal-safe; see signal-safety(7).
              Nevertheless, we use printf() here as a simple way of
              showing that the handler was called. */

           printf("Caught signal %d\n", sig);
           print_siginfo(si);
           signal(sig, SIG_IGN);
       }


int add_user_thread(thread_manager * tmg, void * func, void * args) {
/*
- lock queue mutex
- check if WORK_QUEUE is full, if yes unlock mutex and return -1
- add to queue
- unlock mutex 
- return 0
*/

}

static void * many_to_one_worker(void *arg) {
	struct thread_manager *mgr = arg;
	printf("many_to_one_worker fn");

	timer_t timerid;
	struct sigevent sev;
	struct itimerspec its;
	long long freq_nanosecs;
	sigset_t mask;
	struct sigaction sa;


	/* Establish handler for timer signal */
/*
- register SIGALRM handler 
- initialize POSIX timer
*/

	printf("Establishing handler for signal %d\n", SIG);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIG, &sa, NULL) == -1)
		errExit("sigaction");

           printf("Blocking signal %d\n", SIG);
           sigemptyset(&mask);
           sigaddset(&mask, SIG);
           if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
               errExit("sigprocmask");

           /* Create the timer */

           sev.sigev_notify = SIGEV_SIGNAL;
           sev.sigev_signo = SIG;
           sev.sigev_value.sival_ptr = &timerid;
           if (timer_create(CLOCKID, &sev, &timerid) == -1)
               errExit("timer_create");

           printf("timer ID is 0x%lx\n", (long) timerid);

           /* Start the timer */

           freq_nanosecs = 1000;
           its.it_value.tv_sec = freq_nanosecs / 1000000000;
           its.it_value.tv_nsec = freq_nanosecs % 1000000000;
           its.it_interval.tv_sec = its.it_value.tv_sec;
           its.it_interval.tv_nsec = its.it_value.tv_nsec;

           if (timer_settime(timerid, 0, &its, NULL) == -1)
                errExit("timer_settime");
/* Initialize CONTEXT queue */
/*
	Loop:
- sleep on condition
- check queue for new stuff
- execute head of WORK queue

SIGALRM handler:
if CONTEXT queue and WORK queue are empty, do nothing.
else if WORK queue or CONTEXT queue has members:{
ucontext_t cur_context;
getcontext(&cur_context);
// save cur_context to CONTEXT queue
}

CONTEXT_QUEUE.append(cur_context);

if WORK queue has new members {
// give priority to new WORK
let top = WORK_QUEUE.pop();
top();
} else {
ucontext_t new_context = CONTEXT_QUEUE.pop();
setcontext(new_context);
}
*/
	return;
}



