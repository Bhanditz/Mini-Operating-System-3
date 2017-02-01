#include <iostream>  
#include <iomanip>
#include <fstream>  
#include <pthread.h>  
#include <unistd.h>
#include <queue>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

using namespace std;  

int* sort_array = NULL;
int countNum = 0;
int get_size(char* file);  
void read_file(char* file,int* sort_array);  
void* merge_sort(void* arg);  
void merge(int* arr, int left, int middle, int right); 


// stdout is a shared resource, so protected it with a mutex
static pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t count_mutex_temp = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t countEqlZero;



class Task {
public:
	Task() {}
	virtual void run()=0;
};


class WorkQueue {
public:
	WorkQueue() {
		pthread_mutex_init(&qmtx,0);

		pthread_cond_init(&wcond, 0);
	}

	~WorkQueue() {
		// Cleanup pthreads
		pthread_mutex_destroy(&qmtx);
		pthread_cond_destroy(&wcond);
		pthread_mutex_destroy(&count_mutex);
		pthread_cond_destroy(&countEqlZero);
	}

	Task *nextTask() {
		// The return value
		Task *nt = 0;

		// Lock the queue mutex
		pthread_mutex_lock(&qmtx);
		// Check if there's work
		if (finished && tasks.size()) {
			// If not return null (0)
			nt = 0;
		} else {
			// Not finished, but there are no tasks, so wait for
			// wcond to be signalled
			while (tasks.size()==0) {
				pthread_cond_wait(&wcond, &qmtx);
			}
			// get the next task
			nt = tasks.front();
			tasks.pop();

		}
		// Unlock the mutex and return
		pthread_mutex_unlock(&qmtx);
		return nt;
	}
	// Add a task
	void addTask(Task *nt) {
		if (!finished) {
			pthread_mutex_lock(&qmtx);
			tasks.push(nt);
			pthread_mutex_lock(&count_mutex);
			countNum++;
			pthread_mutex_unlock(&count_mutex);
			pthread_cond_signal(&wcond);
			pthread_mutex_unlock(&qmtx);
		}
	}
	void finish() {
		pthread_mutex_lock(&qmtx);
		finished = true;
		pthread_cond_signal(&wcond);
		pthread_mutex_unlock(&qmtx);
	}

	bool hasWork() {
		return (tasks.size()>0);
	}

private:
	std::queue<Task*> tasks;
	bool finished;
	pthread_mutex_t qmtx;
	pthread_cond_t wcond;
};

// Function that retrieves a task from a queue, runs it and deletes it
static void *getWork(void* param) {
	Task *mw = 0;
	WorkQueue *wq = (WorkQueue*)param;
	while (mw = wq->nextTask()) {

		mw->run();
		delete mw;
		pthread_mutex_lock(&count_mutex);
		countNum--;
		if(countNum == 0)
		{
			pthread_cond_signal(&countEqlZero);
			pthread_mutex_unlock(&count_mutex);

		}
		else
			pthread_mutex_unlock(&count_mutex);
	}
	return 0;
}

class ThreadPool {
public:

	ThreadPool(int n) : _numThreads(n) {
		pthread_t* threads = new pthread_t[n];
		for (int i=0; i< n; ++i) {
			pthread_create(&(threads[i]), 0, getWork, &workQueue);
		}
	}

	~ThreadPool() {
		workQueue.finish();
		waitForCompletion();
		for (int i=0; i<_numThreads; ++i) {
			pthread_join(threads[i], 0);
		}
		delete [] threads;
	}

	void addTask(Task *nt) {
		workQueue.addTask(nt);
	}

	void finish() {
		workQueue.finish();
	}

	bool hasWork() {
		return workQueue.hasWork();
	}
	void waitForCompletion() {
		while (workQueue.hasWork()) {}
	}

private:
	pthread_t *threads;
	int _numThreads;
	WorkQueue workQueue;
};



class SortTask : public Task {
public:
	SortTask(int* arr, int left, int middle, int right) : Task(), arr(arr), left(left), middle(middle), right(right) {}

	virtual void run() {
		long long val = Merge(arr, left, middle, right);
	}

private:

	int Merge(int* arr, int left, int middle, int right) {
		int i, j, k;  
		int half1 = middle - left + 1;  
		int half2 = right - middle;

		int* first = new int [half1];
		int* second = new int [half2+1];  
		for(int i = 0; i < half1; i++)  
		{  
			first[i] = arr[left+i];  
		}  
		for(int i = 0; i < half2; i++)  
		{  
			second[i] = arr[middle+i+1];  
		}  

		i = 0;  
		j = 0;  

		k = left;  

		while(i < half1 && j < half2)  
		{  
			if(first[i] <= second[j])  
			{  
				arr[k] = first[i];  
				i++;  
			}  
			else  
			{  
				arr[k] = second[j];  
				j++;  
			}  
			k++;  
		}  

		while(j < half2)  
		{  
			arr[k] = second[j];  
			j++;  
			k++;  
		}  

		while(i < half1)  
		{  
			arr[k] = first[i];  
			i++;  
			k++;  
		}

		return 0;
	}
	int right;
	int left;
	int middle;
	int* arr;
};



int main(int argc, char*argv[])  
{  
	int sort_len;
	int file_len;
	int powerIndice;

	sort_len = get_size( argv[1] );
	powerIndice = log10(sort_len)/log10(2);
	powerIndice = pow(2, powerIndice) - sort_len < 0 ? powerIndice + 1 : powerIndice;
	file_len = sort_len;
	sort_len = pow(2, powerIndice);

	pthread_mutex_init(&count_mutex,0);
	pthread_mutex_init(&count_mutex_temp,0);
	pthread_cond_init(&countEqlZero,0);

	try  
	{  
		sort_array = new int[sort_len];  
	}catch(std::bad_alloc& e)  
	{  
		cout << "new a integer array failed!" <<endl;  
		return -1;  
	}  

	ThreadPool *tp = new ThreadPool(sort_len);	// create a thread pool

	read_file(argv[1],sort_array);
	for(int i = file_len; i<sort_len; i++)
	{
		sort_array[i] = 9999;
	}
	for( int loop_num = powerIndice; loop_num >= 1; loop_num-- )
	{
		int num = pow(2, loop_num-1);

		// 创建 pow(2, loop_num) 个线程来merge_sort。count(写保护)
		for(int i = 0; i<num; i++)
		{
			int left = i * pow(2, powerIndice-loop_num+1);
			int right = left + pow(2, powerIndice-loop_num+1) - 1;
			int middle = (left+(right-1))/ 2;
			tp->addTask(new SortTask( sort_array, left, middle, right));
		}
		// 判断count是否为0，等待，然后进入下一轮loop。
		pthread_mutex_lock(&count_mutex_temp);
		pthread_cond_wait(&countEqlZero, &count_mutex_temp);
		pthread_mutex_unlock(&count_mutex_temp);
	}

	// Output the sorted array
	for(int i = 0; i < file_len ; i++)  
	{  	
		printf("%5d", sort_array[i]);
		if( i%10 == 9)
			cout << endl;
		else
			cout << ' ';
	}  
	cout << endl;
	tp->finish();

	delete sort_array;  
	return 0;  
}  


int get_size(char* file)  
{  
	int len = 0;  
	int t = 0;  
	ifstream in(file);  
	while(!in.eof())  
	{  
		in >> t;  
		len++;  
	}  
	return len - 1;  
}  


void read_file(char* file,int* sort_array)  
{  
	ifstream in(file);  
	int i = 0;  
	while(!in.eof())  
	{
		if(in.dec != '\n')
			in >> *(sort_array+i);  
		i++;
	}  
}  

