#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "uthread.h"

#define uthread_RUNNING 0 /* the thread is running */
#define uthread_CANCEL 1 /* the thread is cancelled */
#define uthread_DONE 2 /* the thread has done */
#define TIME_SLICE 1000

 

/* global data section */
static steque_t ready_queue[10];
static steque_t zombie_queue;
static thread_t* current;
static struct itimerval timer;
sigset_t vtalrm;
static uthread_t maxtid; 
static int inited_flag = 0;

extern sigset_t vtalrm;

/* private functions prototypes */
void sigvtalrm_handler(int sig);
void uthread_start(void* (*start_routine)(void*), void* args);
//thread_t* thread_get(uthread_t tid);


int uthread_mutex_init(uthread_mutex_t* mutex)
{
    sigprocmask(SIG_BLOCK, &vtalrm, NULL); /* in case this is blocked previously */    
    steque_init(mutex);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
    return 0;
}

int uthread_mutex_lock(uthread_mutex_t* mutex){
    sigprocmask(SIG_BLOCK, &vtalrm, NULL); 

    /* if queue lock is empty */
    if (steque_isempty(mutex))
    {
        steque_enqueue(mutex, (steque_item) uthread_self());  
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);   
        return 0;
    }

    /* if a thread try to acquire lock */ 
    if (uthread_self() == (uthread_t) steque_front(mutex))
    {
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
        return 0;
    }

    steque_enqueue(mutex, (steque_item) uthread_self()); 
    while (uthread_self() != (uthread_t) steque_front(mutex)) 
    {
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
        /* actively perform context switching */
        sigvtalrm_handler(SIGVTALRM);
        sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    }
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);  
    return 0; 
}


int uthread_mutex_unlock(uthread_mutex_t *mutex){
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    if (steque_isempty(mutex))
    {
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
        return -1;
    }

    if ((uthread_t) steque_front(mutex) != uthread_self())
    {
       sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
       return -1;
    }

    steque_pop(mutex);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
    return 0; 
}

int uthread_mutex_destroy(uthread_mutex_t *mutex){
    sigprocmask(SIG_BLOCK, &vtalrm, NULL); /* in case this is blocked previously */    
    steque_destroy(mutex);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
    return 0; 
}







void uthread_init()
{
	if(inited_flag == 1)
	{
		printf("The libs have already been initialized\nContinue...\n");
		return -1;
	}
	int period = TIME_SLICE;
    struct sigaction act;

    /* initializing data structures */
    maxtid = 1;
	for(int i = 0; i<10 ; i++)
	{    steque_init(&(ready_queue[i]));
		 //printf("init queue[%d] size: %d\n", i, steque_size(&(ready_queue[i])));
	}
    /* create main thread and add it to ready queue */  
    /* only main thread is defined on heap and can be freed */
    thread_t* main_thread = (thread_t*) malloc(sizeof(thread_t));
    main_thread->tid = maxtid++;
    main_thread->ucp = (ucontext_t*) malloc(sizeof(ucontext_t)); 
    memset(main_thread->ucp, '\0', sizeof(ucontext_t));
    main_thread->arg = NULL;
    main_thread->state = uthread_RUNNING;
    main_thread->joining = 0;

    /* must be called before makecontext */
    if (getcontext(main_thread->ucp) == -1)
    {
      perror("getcontext");
      exit(EXIT_FAILURE);
    }

    current = main_thread;
    
    /* setting uo the signal mask */
    sigemptyset(&vtalrm);
    sigaddset(&vtalrm, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); /* in case this is blocked previously */

    /* set alarm signal and signal handler */
    timer.it_interval.tv_usec = period;
    timer.it_interval.tv_sec = 0;
    timer.it_value.tv_usec = period;
    timer.it_value.tv_sec = 0; 
    
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0)
    {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }

    /* install signal handler for SIGVTALRM */  
    memset(&act, '\0', sizeof(act));
    act.sa_handler = &sigvtalrm_handler;
    if (sigaction(SIGVTALRM, &act, NULL) < 0)
    {
      perror ("sigaction");
      exit(EXIT_FAILURE);
    }
}


/*
  The uthread_create() function mirrors the pthread_create() function,
  only default attributes are always assumed.
 */
int uthread_create(void (*func)(int), int val, int pri)
{
    /* block SIGVTALRM signal */
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    
    /* allocate heap for thread, it cannot be stored on stack */
    thread_t* t = malloc(sizeof(thread_t));
    t->tid = maxtid++; // need to block signal
    t->state = uthread_RUNNING;
    t->pri = pri;
    t->arg = val;
    t->ucp = (ucontext_t*) malloc(sizeof(ucontext_t));
    t->joining = 0;
    memset(t->ucp, '\0', sizeof(ucontext_t));

    if (getcontext(t->ucp) == -1)
    {
      perror("getcontext");
      exit(EXIT_FAILURE);
    }
    
    /* allocate stack for the newly created context */
    /* here we allocate the stack size using the canonical */
    /* size for signal stack. */
    t->ucp->uc_stack.ss_sp = malloc(1048576);
    t->ucp->uc_stack.ss_size = 1048576;
    t->ucp->uc_stack.ss_flags = 0;
    t->ucp->uc_link = 0;
	//printf("before make context val:%d", val);

    makecontext(t->ucp, (void(*)())(func), 1, val);
    steque_enqueue(&(ready_queue[pri/10]), t);
    //printf("readyqueue size: %d    pri:%d\n", steque_size(&(ready_queue[pri/10])), pri/10);
    /* unblock the signal */
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);   

    return t->tid;
}

/*
  The uthread_join() function is analogous to pthread_join.
  All uthreads are joinable.
 */
int uthread_join(uthread_t thread, int* val)
{
    /* if a thread tries to join itself */
    if (thread == current->tid)
        return -1;

    thread_t* t;
    /* if a thread is not created */
    if ((t = thread_get(thread)) == NULL)
        return -1;

    /* check if that thread is joining on me */
    if (t->joining == current->tid)
        return -1;

    current->joining = t->tid;
    /* wait on the thread to terminate */
    while (t->state == uthread_RUNNING)
    {
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
        sigvtalrm_handler(SIGVTALRM);
        sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    }


    if (t->state == uthread_CANCEL)
        *val = 0;
    else if (t->state == uthread_DONE)
    {
        t->retval = (int) 0;
        *val = 0;
    }


    return 0;
}

/*
  The uthread_exit() function is analogous to pthread_exit.
 */
void uthread_exit(void* retval)
{
    /* block alarm signal */
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
	int sum = 0;
	// All the ready queues are empty
    for(int i = 0; i<10 ; i++)
	{
		if(steque_isempty(&(ready_queue[i])))
			sum += (i+1);
		if(sum == 55)
		{
			sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
			exit((int) retval);
		}
	}


    /* if the main thread call uthread_exit */
    if (current->tid == 1)
    {
        while (!
				(steque_isempty(&(ready_queue[0])) && steque_isempty(&(ready_queue[1])) && steque_isempty(&(ready_queue[2])) && steque_isempty(&(ready_queue[3])) 
				&& steque_isempty(&(ready_queue[4])) && steque_isempty(&(ready_queue[5])) && steque_isempty(&(ready_queue[6])) && steque_isempty(&(ready_queue[7])) 
				&& steque_isempty(&(ready_queue[8])) && steque_isempty(&(ready_queue[9]))))
        {
            sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);  
            sigvtalrm_handler(SIGVTALRM);
            sigprocmask(SIG_BLOCK, &vtalrm, NULL);
        }
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);   
        exit((int) retval);
    }

    thread_t* prev = current; 
	//thread_t* temp_thread = steque_pop(&(ready_queue[current->pri/10]));
    //printf(" in uthread_exit ready_queue[0] size: %d\n", steque_size(&ready_queue[0]));
    for(int j = 0, flag = 0; j<10; j++) {
        current = (thread_t*) steque_top_pop(&ready_queue[j], &flag);
        if( flag == 1)
        {
            //printf(" switched to ready_queue[%d]\n", j);
            break;
        }
    }
	//printf("\n\n after getting top of another queue \n\n");
    current->state = uthread_RUNNING;

    /* free up memory allocated for exit thread */
    /* mark the exit thread as DONE and add to zombie_queue */ 
    prev->state = uthread_DONE; 
    prev->retval = retval;
    prev->joining = 0;
    steque_enqueue(&zombie_queue, prev);

    /* unblock alarm signal and setcontext for next thread */
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
    setcontext(current->ucp);
}

/*
  The uthread_yield() function is analogous to pthread_yield, causing
  the calling thread to relinquish the cpu and place itself at the
  back of the schedule queue.
 */
int uthread_yield(void)
{
    /* block SIGVTALRM signal */
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    
    /* if no thread to yield, simply return */
    if ((steque_isempty(&(ready_queue[0])) && steque_isempty(&(ready_queue[1])) && steque_isempty(&(ready_queue[2])) && steque_isempty(&(ready_queue[3])) 
				&& steque_isempty(&(ready_queue[4])) && steque_isempty(&(ready_queue[5])) && steque_isempty(&(ready_queue[6])) && steque_isempty(&(ready_queue[7])) 
				&& steque_isempty(&(ready_queue[8])) && steque_isempty(&(ready_queue[9]))))
        return 0;

	steque_enqueue(&(ready_queue[current->pri/10]), current);
    thread_t* next = (thread_t*) steque_pop(&(ready_queue[current->pri/10]));
    thread_t* prev = current;
    current = next;

    /* unblock the signal */
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
    swapcontext(prev->ucp, current->ucp); 
    return 0; 
}

int  uthread_equal(uthread_t t1, uthread_t t2)
{
    return t1 == t2;
}

/*
  Returns calling thread.
 */
uthread_t uthread_self(void)
{
    return current->tid;
}


void uthread_start(void* (*start_routine)(void*), void* args)
{
    /* unblock signal comes from uthread_create */
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);

    /* start executing the start routine*/
    current->retval = (*start_routine)(args);

    /* when start_rountine returns, call uthread_exit*/
    uthread_exit(current->retval);
}

/*
 * A signal handler for SIGVTALRM
 * Comes here when a thread runs up its time slot. This handler implements
 * a preemptive thread scheduler. It looks at the global ready queue, pop
 * the thread in the front, save the current thread context and switch context. 
 */
void sigvtalrm_handler(int sig)
{
    /* block the signal */
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);

    /* if no thread in the ready queue, resume execution */
    if (steque_isempty(&(ready_queue[0])) && steque_isempty(&(ready_queue[1])) && steque_isempty(&(ready_queue[2])) && steque_isempty(&(ready_queue[3])) 
				&& steque_isempty(&(ready_queue[4])) && steque_isempty(&(ready_queue[5])) && steque_isempty(&(ready_queue[6])) && steque_isempty(&(ready_queue[7]) )
				&& steque_isempty(&(ready_queue[8])) && steque_isempty(&(ready_queue[9])))
        return;

    /* get the next runnable thread and use preemptive scheduling */
    thread_t* next = (thread_t*) steque_pop(&(ready_queue[current->pri/10]));
    while (next->state == uthread_CANCEL)
    {
        steque_enqueue(&zombie_queue, next);
        next = (thread_t*) steque_pop(&(ready_queue[current->pri/10])); 
    }
    thread_t* prev = current;
    steque_enqueue(&(ready_queue[current->pri]), current);
    next->state = uthread_RUNNING; 
    current = next;

    /* unblock the signal */
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL); 
    swapcontext(prev->ucp, current->ucp);
}



void steque_init(steque_t *this){
  this->front = NULL;
  this->back = NULL;
  this->N = 0;
}

void steque_enqueue(steque_t* this, steque_item item){
  steque_node_t* node;

  node = (steque_node_t*) malloc(sizeof(steque_node_t));
  node->item = item;
  node->next = NULL;
  
  if(this->back == NULL)
    this->front = node;
  else
    this->back->next = node;

  this->back = node;
  this->N += 1;
  //printf("STEQUE_ENQUE  %d  \n", this->N);
}

void steque_push(steque_t* this, steque_item item){
  steque_node_t* node;

  node = (steque_node_t*) malloc(sizeof(steque_node_t));
  node->item = item;
  node->next = this->front;

  if(this->back == NULL)
    this->back = node;
  
  this->front = node;
  this->N += 1;
  printf("PUSH    %d\n", this->N);
}

int steque_size(steque_t* this){
  return this->N;
}

int steque_isempty(steque_t *this){
  return this->N == 0;
}



steque_item steque_pop(steque_t* this){
  steque_item ans;
  steque_node_t* node;
  
  if(this->front == NULL){
    fprintf(stderr, "Error: underflow in steque_pop.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }

  node = this->front;
  ans = node->item;

  this->front = this->front->next;
  if (this->front == NULL) this->back = NULL;
  free(node);

  this->N -= 1;
  return ans;
}

steque_item steque_top_pop(steque_t* this, int* flag){
  steque_item ans;
  steque_node_t* node;


    //printf("\n\nin steque_top_pop, stequeue_size: %d\n\n", steque_size(this));
    if(steque_isempty(this))
    {
        this->back = NULL;
        return NULL;
    }
    if(this->front == NULL){
	    fprintf(stderr, "Error: underflow in steque_pop.\n");
	    fflush(stderr);
	    exit(EXIT_FAILURE);
    }
	 //
    node = this->front;
    ans = node->item;

    this->front = this->front->next;
    if (this->front == NULL) this->back = NULL;
    free(node);

    this->N -= 1;
    *flag = 1;
    return ans;
}



void steque_cycle(steque_t* this){
  if(this->back == NULL)
    return;
  
  this->back->next = this->front;
  this->back = this->front;
  this->front = this->front->next;
  this->back->next = NULL;
}

steque_item steque_front(steque_t* this){
  if(this->front == NULL){
    fprintf(stderr, "Error: underflow in steque_front.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }
  
  return this->front->item;
}

void steque_destroy(steque_t* this){
  while(!steque_isempty(this))
    steque_pop(this);
}

thread_t* thread_get(uthread_t tid)
{
    steque_node_t* current = ready_queue[0].front;
    while (current != NULL)
    {
        thread_t* t= (thread_t*) current->item;
        if (t->tid == tid)
            return t;
        current = current->next;
    }

    current = zombie_queue.front;
    while (current != NULL)
    {
        thread_t* t= (thread_t*) current->item;
        if (t->tid == tid)
            return t;
        current = current->next;
    }
    return NULL;
}


int usem_init(usem_t *sem, int pshared, unsigned value)
{

}

int usem_destroy(usem_t *sem)
{

}