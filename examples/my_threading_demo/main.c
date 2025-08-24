
/*
	Demo of race condition and how to fix it with mutex
*/
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

//shared data between threads
int balance = 0;

void* deposit(void* amount);
pthread_mutex_t mutex;

int read_balance()
{
	usleep(200000);
	return balance;
}

void write_balance(int new_balance)
{
	usleep(200000);
	balance = new_balance;
}

int main()
{
	printf("initial balance: %d\n", read_balance());

	//create thread
	pthread_t thread1, thread2;
	
	//init mutex
	pthread_mutex_init(&mutex, NULL);

	int deposit1 = 1000;
	int deposit2 = 500;

	//create a new thread
	pthread_create(&thread1, NULL, deposit, (void*) &deposit1);
	pthread_create(&thread2, NULL, deposit, (void*) &deposit2);

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	
	//destroy mutex
	pthread_mutex_destroy(&mutex);

	printf("final balance: %d\n", read_balance());
}

//thread function
void* deposit(void *amount)
{
	//create lock
	pthread_mutex_lock(&mutex);

	int acc_balance = read_balance();
	acc_balance += *((int*) amount);
	write_balance(acc_balance);

	//release lock
	pthread_mutex_unlock(&mutex);

	return NULL;
}