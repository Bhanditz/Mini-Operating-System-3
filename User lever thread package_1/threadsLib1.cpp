#include <stdio.h>  
#include <queue>
#include <ucontext.h>
#include <cstdlib>
#include "threadsLib1.h"
using namespace std;

#define STACK_SIZE 1048576


ucontext_t* context_t;
int inited_flag = 0;

class tQueue {
public:
	tQueue()
	{
	}
	void pop()
	{
		thread_queue.pop();
	}

	void push(ucontext_t* context)
	{
		thread_queue.push(context);
	}

	ucontext_t* front()
	{
		return thread_queue.front();
	}

	void addThread(ucontext_t* threadID)
	{
		thread_queue.push(threadID);
	}

	int size()
	{
		return thread_queue.size();
	}
	
	ucontext_t* back()
	{
		return thread_queue.back();
	}
private:
	queue<ucontext_t*> thread_queue;

};

tQueue readyQueue;

int uthread_init()
{
	// already initialized
	if(inited_flag == 1)
	{
		printf("The libs are already initialized\nContinue...\n");
		return -1;
	}


	// initialize arguments here;
	else
	{
		ucontext_t* newThread = new ucontext_t;
		getcontext(newThread);
		readyQueue.addThread(newThread);

		inited_flag = 1;
	}




}

int uthread_create(void (*func)(int), int val, int pri)
{
	// create a thread	and  setcontext
	ucontext_t* newThread = new ucontext_t;
	getcontext(newThread);
	newThread->uc_link=0;
	newThread->uc_stack.ss_sp=new char [STACK_SIZE];
	newThread->uc_stack.ss_size=STACK_SIZE;
	newThread->uc_stack.ss_flags=0;
	
	makecontext(newThread, (void(*)())(func), val);

	// add the thread to the ready queue
	readyQueue.addThread(newThread);

}

int uthread_yield(void)
{
	// add to the tail of ready queue
	ucontext_t* newThread = readyQueue.front();
	readyQueue.addThread(newThread);
	// change to the next thread;
	readyQueue.pop();
	ucontext_t* temp = readyQueue.front();
	//setcontext(temp);
	swapcontext(readyQueue.back(), readyQueue.front());
}

void uthread_exit(void *retval)
{
	readyQueue.pop();
	if(readyQueue.size()> 0)
	{
		ucontext_t* temp = new ucontext_t;
		temp = readyQueue.front();
		setcontext(temp);
	}
	else
	{
		exit(0);
	}
}

