// header file for project 2

int uthread_init(void);
int uthread_create(void(*func)(int), int val, int pri);
int uthread_yield(void);
void uthread_exit(void *retval);