#ifndef __uthread_H
#define __uthread_H

#include <ucontext.h>
#include <semaphore.h>

typedef unsigned int uthread_t;
typedef uthread_t uthread_tid_t;
typedef void* steque_item;
typedef sem_t usem_t;



typedef struct steque_node_t{
  steque_item item;
  struct steque_node_t* next;
} steque_node_t;


typedef struct {
  steque_node_t* front;
  steque_node_t* back;
  int N;
}steque_t;

typedef steque_t uthread_mutex_t; 

typedef struct Thread_t
{
    uthread_t tid;
    uthread_t joining;
    int state;
	int pri;
    int arg;
    int* retval;
    ucontext_t* ucp; 
} thread_t; 

/* Initializes the data structure */
void steque_init(steque_t* this);

/* Return 1 if empty, 0 otherwise */
int steque_isempty(steque_t* this);

/* Returns the number of elements in the steque */
int steque_size(steque_t* this);

/* Adds an element to the "back" of the steque */
void steque_enqueue(steque_t* this, steque_item item);

/* Adds an element to the "front" of the steque */
void steque_push(steque_t* this, steque_item item);

/* Removes an element to the "front" of the steque */
steque_item steque_pop(steque_t* this);
steque_item steque_top_pop(steque_t* this, int* flag);
/* Removes the element on the "front" to the "back" of the steque */
void steque_cycle(steque_t* this);

/* Returns the element at the "front" of the steque without removing it*/
steque_item steque_front(steque_t* this);

/* Empties the steque and performs any necessary memory cleanup */
void steque_destroy(steque_t* this);






void uthread_init();

int  thread_create(void (*func)(int),
					 int val, 
					 int pri);

int  uthread_join(uthread_t tid, int *retval);

void uthread_exit(void *retval);

int uthread_yield(void);

int  uthread_equal(uthread_t t1, uthread_t t2);

int  uthread_cancel(uthread_t thread);

uthread_t uthread_self(void);
thread_t* thread_get(uthread_t tid);
int  uthread_mutex_init(uthread_mutex_t *mutex);
int  uthread_mutex_lock(uthread_mutex_t *mutex);
int  uthread_mutex_unlock(uthread_mutex_t *mutex);
int  uthread_mutex_destroy(uthread_mutex_t *mutex);

void sigvtalrm_handler(int sig);



int usem_init(usem_t *sem, int pshared, unsigned value);

int usem_destroy(usem_t *sem);

int usem_wait(usem_t *sem);

int usem_post(usem_t *sem);

#endif // __uthread_H