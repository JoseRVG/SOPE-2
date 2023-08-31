#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>



int main()
{
	/*if(mkfifo("FIF",0660) == -1)
	{
		perror("mkfifo");
		return 1;
	}

	printf("fifo created\n");
	int fd = open("FIF",O_WRONLY);
	if(fd == -1)
	{
		perror("open");
		return 1;

	}

	printf("func ended \n");*/

	sem_t* sem = sem_open("/sem_park",O_CREAT,0600,0);
	if(sem == SEM_FAILED)
	{
		perror("sem_open");
		return 1;
	}

	if(sem_wait(sem) != 0)
	{
		printf("sem_wait failed");
	}

	printf("func ended \n");

	return 0;
}


