// test case 2 for project 2

#include <stdio.h>
#include <unistd.h>
#include "threadsLib1.h"

void threadFunc(int);

int* returnValue;

int main() {

	int i;

	uthread_init();

	uthread_create(threadFunc, 1, 0);
	uthread_create(threadFunc, 2, 0);
	uthread_create(threadFunc, 3, 0);
	uthread_create(threadFunc, 4, 0);

	for (i = 0; i < 10; i++) { 
		printf("this is the main function\n");
		uthread_yield();
	}
	printf("in main now and exiting\n");
	uthread_exit(&returnValue);
	return 0;
}

void threadFunc(int val) {

	int i, j;

	for (i = 0; i < 5; i++) {
		printf("thread: %d count: %d\n", val, i);
		for (j = 0; j < 10000000; j++);
		uthread_yield();
	}
	uthread_exit(&returnValue);
}
