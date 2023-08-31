#include <stdio.h> //printf
#include <sys/types.h> //open
#include <sys/stat.h> //open
#include <fcntl.h> //open
#include <signal.h> //pthread_sigmask
#include <stdlib.h> //malloc, free
#include <unistd.h> //read, write, unlink, sleep
#include <errno.h> //errno
#include <semaphore.h> //sem_open

#include "parque.h"

//Global variables (shared amongst all of the park's threads)
long int n_spots;
long int n_used;
long int total_cars;
long int parked_cars;
long int max_cars;
pthread_mutex_t mut=PTHREAD_MUTEX_INITIALIZER; //mutex used to control the n_used variable

void writeToParkLog(clock_t cur_ticks, struct park_request req, long int cur_n_used, char park_observ[])
{
	//Step 1: Open the log file
	int log_fd;
	if((log_fd = open(PARK_LOG,O_WRONLY|O_APPEND)) == -1)
	{
		perror("open of park log file");
		return;
	}

	//Step 2: Define the line to write to the log file
	char line[PARK_LOG_LINE_SIZE];
	sprintf(line,"\n%15u ; %15u ; %15u ; %15c ; %20s",
			(unsigned int)(cur_ticks),(unsigned int)(cur_n_used),
			(unsigned int)req.car_id,req.gate,
			park_observ);

	//Step 3: Write down on the park log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write to the park log file");
		close(log_fd);
		return;
	}

	//Step 4: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close of the park log file");
	}

	return;
}

void *usher_thr(void *arg)
{
	//Step 1: Define the thread as detached
	if(pthread_detach(pthread_self()) != 0)
	{
		printf("pthread_detach failed\n");
	}

	//Step 2: Retrieve thread's arguments
	struct usher_thr_args *args = (struct usher_thr_args*)arg;
	struct park_request req = args->req;

	//Step 3: Open car's private FIFO
	int fd_xmit = open(req.fifo_name,O_WRONLY);
	if(fd_xmit == -1)
	{
		perror("open");
	}

	//Step 4: Check if there is space for the car on the park
	int has_space = 0; //used to decide if the car can enter the park (n_used < n_spots)
	clock_t cur_ticks;
	long int cur_n_used; //used to read the n_used variable at a given moment

	int ret = pthread_mutex_lock(&mut);
	if(ret != 0)
	{
		printf("pthread_mutex_lock failed with code %d\n",ret);
	}
	//////// Start of critical section for finding a spot ////////
	total_cars++;
	if(n_used < n_spots)
	{
		//Update the n_used variable
		n_used++;
		has_space = 1;

		parked_cars++;

		if(n_used > max_cars)
		{
			max_cars = n_used;
		}
	}
	clock_t end = times(NULL);
	cur_ticks = (end - args->start);
	cur_n_used = n_used;
	//////// End of critical section for finding a spot ////////
	ret = pthread_mutex_unlock(&mut);
	if(ret != 0)
	{
		printf("pthread_mutex_unlock failed with code %d\n",ret);
	}

	//Step 5: Give an answer to the car thread
	struct park_answer ans;
	if(has_space)
	{
		//Step 5a: Write to the log file
		writeToParkLog(cur_ticks,req,cur_n_used,PARK_ENTRY_OBSERV);

		//Step 5b: Give an answer to the car thread
		ans.response = ENTRY_RESP;
		if(write(fd_xmit,&ans,sizeof(ans)) == -1)
		{
			perror("write");
		}

		//Step 5c: Wait for the number of clock ticks defined on req.park_time
		const long int ticks_per_sec = sysconf(_SC_CLK_TCK);
		if(ticks_per_sec == -1)
		{
			perror("sysconf");
		}
		const double micros_per_tick = 1000000/((double)ticks_per_sec);
		if(usleep(req.park_time*micros_per_tick) == -1)
		{
			perror("usleep");
		}

		//Step 5d: Free the parking spot
		ret = pthread_mutex_lock(&mut);
		if(ret != 0)
		{
			printf("pthread_mutex_lock failed with code %d\n",ret);
		}
		//////// Start of critical section for freeing a spot ////////
		//Update the n_used variable
		n_used--;
		end = times(NULL);
		cur_ticks = (end - args->start);
		cur_n_used = n_used;
		//////// End of critical section for freeing a spot ////////
		ret = pthread_mutex_unlock(&mut);
		if(ret != 0)
		{
			printf("pthread_mutex_unlock failed with code %d\n",ret);
		}

		//Step 5e: Write to the log file
		writeToParkLog(cur_ticks,req,cur_n_used,PARK_EXIT_OBSERV);

		//Step 5f: Give an answer to the car thread
		ans.response = EXIT_RESP;
		if(write(fd_xmit,&ans,sizeof(ans)) == -1)
		{
			perror("write");
		}
	}
	else
	{
		//Step 5A: Write to the log file
		writeToParkLog(cur_ticks,req,cur_n_used,PARK_FULL_OBSERV);

		//Step 5B: Give an answer to the car thread
		ans.response = FULL_RESP;
		if(write(fd_xmit,&ans,sizeof(ans)) == -1)
		{
			perror("write");
		}
	}

	//Step 6: Close car's private FIFO
	if(close(fd_xmit) == -1)
	{
		perror("close of car private FIFO");
	}

	//Step 7: Free the argument memory
	free(arg);

	pthread_exit(NULL);
}

void *gate_thr(void *arg)
{
	//Step 1: Retrieve thread's arguments
	struct gate_thr_args *args = (struct gate_thr_args*)arg;
	char gate = args->gate;

	//Step 2: Obtain the rcvr FIFO name
	char rcvr_fifo_name[GATE_FIFO_NAME_LEN];
	switch(gate)
	{
	case NORTH_GATE:
		strcpy(rcvr_fifo_name,NORTH_GATE_FIFO);
		break;
	case SOUTH_GATE:
		strcpy(rcvr_fifo_name,SOUTH_GATE_FIFO);
		break;
	case WEST_GATE:
		strcpy(rcvr_fifo_name,WEST_GATE_FIFO);
		break;
	case EAST_GATE:
		strcpy(rcvr_fifo_name,EAST_GATE_FIFO);
		break;
	}

	//Step 3: Open the FIFO gate on read-only mode
	int fd_rcvr = open(rcvr_fifo_name,O_RDONLY);
	if(fd_rcvr == -1)
	{
		perror("open");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 4: Read park_requests (and the signal to close the park) while the park is open
	int park_is_open = 1;
	while(park_is_open)
	{
		//Step 4a: Read the next request
		struct park_request req;
		if(read(fd_rcvr,&req,sizeof(struct park_request)) == -1)
		{
			perror("read");
			close(fd_rcvr);
			free(arg);
			pthread_exit(NULL);
		}

		//Step 4b: Check if the request is actually the notification to close the park
		if(req.gate == CLOSE_PARK)
		{
			park_is_open = 0;
		}
		else
		{
			//Step 4c: Create the "usher" (= "arrumador") thread
			struct usher_thr_args *thr_arg = (struct usher_thr_args *)malloc(sizeof(struct usher_thr_args));
			if(thr_arg == NULL)
			{
				printf("malloc failed for usher_thr_arg\n");
				continue;
			}
			thr_arg->req = req;
			thr_arg->start = args->start;

			pthread_t tid;

			if(pthread_create(&tid,NULL,usher_thr,thr_arg) != 0)
			{
				printf("pthread_create failed\n");
			}
		}
	}

	//Step 5: Close rcvr FIFO
	if(close(fd_rcvr) == -1)
	{
		perror("close");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 6: Open the log file
	int log_fd;
	if((log_fd = open(PARK_LOG,O_WRONLY|O_APPEND)) == -1)
	{
		perror("open of generator logfile");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 7: Write the log file's header
	char line[PARK_LOG_LINE_SIZE];
	clock_t end = times(NULL);
	clock_t cur_ticks = end - args->start;
	sprintf(line,"\n%15u ; %15u ; %15s ; %15c ; %20s",
			(unsigned int)(cur_ticks),(unsigned int)(n_spots),
			UNDEFINED_VALUE,gate,
			PARK_CLOSED_OBSERV);

	//Step 8: Write down on the generator log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
		close(log_fd);
		free(arg);
		pthread_exit(NULL);
	}

	//Step 9: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 10: Free the argument memory
	free(arg);

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	//Step 1: Check command line arguments
	if(argc != 3)
	{
		printf("Uso: parque <N_LUGARES> <T_ABERTURA>\n");
		return 1;
	}

	errno = 0;
	/*"the calling program should set errno to 0 before the call, and then deter‐
	    mine if an error occurred by checking whether errno has a nonzero value
	    after the call." (man strtol)*/
	n_spots = strtol(argv[1],NULL,10);
	if(errno != 0)
	{
		perror("strtol for T_GERACAO");
		pthread_exit(NULL);
	}
	if(n_spots <= 0)
	{
		printf("N_LUGARES must contain a positive value\n");
		pthread_exit(NULL);
	}

	errno = 0;
	const long int t_aber_secs = strtol(argv[2],NULL,10);
	if(errno != 0)
	{
		perror("strtol for U_RELOGIO");
		pthread_exit(NULL);
	}
	if(t_aber_secs <= 0)
	{
		printf("T_ABERTURA must contain a positive value\n");
		pthread_exit(NULL);
	}

	//Step 2: Initialize the several variables
	//For time tracking
	clock_t start = times(NULL);
	//Global variables
	n_used = 0;
	total_cars = 0;
	parked_cars = 0;
	max_cars = 0;

	//Step 3: Ignore SIGPIPE (in case a thread tries to write to a FIFO after it closed but right before it is destroyed)
	struct sigaction action;
	action.sa_handler = SIG_IGN;
	if(sigemptyset(&(action.sa_mask)) == -1)
	{
		perror("sigemptyset");
		pthread_exit(NULL);
	}
	action.sa_flags = 0;

	if(sigaction(SIGPIPE,&action,NULL) == -1)
	{
		perror("sigaction");
		pthread_exit(NULL);
	}


	//Step 4: Create the four semaphores
	char *gate_sem_names[NUM_GATES] = GATE_SEMS;
	sem_t *sems[NUM_GATES];
	unsigned int i;
	for(i = 0; i < NUM_GATES; i++)
	{
		sems[i] = sem_open(gate_sem_names[i],O_CREAT,PERMISSIONS,1);
		if(sems[i] == SEM_FAILED)
		{
			perror("sem_open failed");
		}
	}

	//Step 5: Create the four gate FIFOS
	char *gate_fifo_names[NUM_GATES] = GATE_FIFOS;
	for(i = 0; i < NUM_GATES; i++)
	{
		if(mkfifo(gate_fifo_names[i],PERMISSIONS) == -1)
		{
			perror("mkfifo");
		}
	}

	//Step 6: Create the log file
	int log_fd;
	if((log_fd = open(PARK_LOG,O_WRONLY|O_CREAT|O_TRUNC,PERMISSIONS)) == -1)
	{
		perror("open of generator logfile (first time)");
		pthread_exit(NULL);
	}

	//Step 7: Write the log file's header
	char line[PARK_LOG_LINE_SIZE];
	sprintf(line,"%15s ; %15s ; %15s ; %15s ; %20s",
			"t(ticks)","nlug",
			"id_viat", "destin",
			"observ");

	//Step 8: Write down on the park log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
		pthread_exit(NULL);
	}

	//Step 9: Write a log entry to note down the absolute number of clock ticks at which the process started
	sprintf(line,"\n%15u ; %15s ; %15s ; %15s ; %20s",
			(unsigned int)(start),UNDEFINED_VALUE,
			UNDEFINED_VALUE, UNDEFINED_VALUE,
			"abertura");

	//Step 10: Write down on the park log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
		pthread_exit(NULL);
	}

	//Step 11: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close");
		pthread_exit(NULL);
	}

	//Step 12: Create each of the gate's threads
	char gates[] = GATES;
	pthread_t tids[NUM_GATES];
	for(i = 0; i < NUM_GATES; i++)
	{
		struct gate_thr_args *thr_arg = (struct gate_thr_args *)malloc(sizeof(struct gate_thr_args));
		if(thr_arg == NULL)
		{
			printf("malloc failed\n");
			continue;
		}
		thr_arg->gate = gates[i];
		thr_arg->start = start;

		if(pthread_create(&tids[i],NULL,gate_thr,thr_arg) != 0)
		{
			printf("pthread_create failed\n");
			if(unlink(gate_fifo_names[i]) == -1)
			{
				perror("unlink");
			}
		}
	}

	//Step 13: Open the four FIFO gates on write-only mode
	int fifo_fd[NUM_GATES];
	for(i = 0; i < NUM_GATES; i++)
	{
		fifo_fd[i] = open(gate_fifo_names[i],O_WRONLY);
		if(fifo_fd[i] == -1)
		{
			perror("open");
		}
	}

	//Step 14: Sleep until it is time to close the park
	sleep(t_aber_secs);

	//Step 15: Signal all four gates that the park will close (by writing to their FIFOS)
	for(i = 0; i < NUM_GATES; i++)
	{
		int ret = sem_wait(sems[i]);
		if(ret == -1)
		{
			perror("sem_wait");
		}
		//////// Start of critical section for closing the park ////////
		struct park_request stop_vehicle;
		stop_vehicle.gate = CLOSE_PARK;
		//Write the FIFO
		if(write(fifo_fd[i],&stop_vehicle,sizeof(struct park_request)) == -1)
		{
			perror("write");
		}
		//Close the FIFO
		if(close(fifo_fd[i]) == -1)
		{
			perror("close");
		}
		//Wait for the controller to end
		if(pthread_join(tids[i],NULL) != 0)
		{
			printf("pthread_join failed\n");
		}
		//////// End of critical section for closing the park ////////
		ret = sem_post(sems[i]);
		if(ret != 0)
		{
			perror("sem_post");
		}
	}

	//Step 16: Destroy the FIFOS
	for(i = 0; i < NUM_GATES; i++)
	{
		if(unlink(gate_fifo_names[i]) == -1)
		{
			perror("unlink");
		}
	}

	//Step 17: Close the semaphores
	for(i = 0; i < NUM_GATES; i++)
	{
		if(sem_close(sems[i]) != 0)
		{
			perror("sem_close");
		}
	}

	//Step 18: Destroy the semaphores
	//"The semaphore is destroyed once all other processes that have the semaphore open close it." (man sem_unlink).
	for(i = 0; i < NUM_GATES; i++)
	{
		if(sem_unlink(gate_sem_names[i]) != 0)
		{
			perror("sem_unlink");
		}
	}

	//Step 19: Retrieve some global statistics
	int ret = pthread_mutex_lock(&mut);
	if(ret != 0)
	{
		printf("pthread_mutex_lock failed with code %d\n",ret);
	}
	//////// Start of critical section for reading some global shared variables ////////
	long int cur_total_cars = total_cars;
	long int cur_parked_cars = parked_cars;
	long int cur_max_cars = max_cars;
	//////// End of critical section for reading some global shared variables ////////
	ret = pthread_mutex_unlock(&mut);
	if(ret != 0)
	{
		printf("pthread_mutex_unlock failed with code %d\n",ret);
	}

	//Step 20: Open the log file
	if((log_fd = open(PARK_LOG,O_WRONLY|O_APPEND)) == -1)
	{
		perror("open of park log file");
	}

	//Step 21: Write a log entry to note down the absolute number of clock ticks at which the process ended
	clock_t end = times(NULL);
	sprintf(line,"\n%15u ; %15s ; %15s ; %15s ; %20s",
			(unsigned int)(end),UNDEFINED_VALUE,
			UNDEFINED_VALUE, UNDEFINED_VALUE,
			"fecho");

	//Step 22: Write down on the park log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
	}

	//Step 23: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close");
	}

	//Step 24: Print out some global statistics
	printf("Park process has successfully ended.\n");
	printf("The total number of cars that accessed the park is %ld.\n",cur_total_cars);
	printf("The number of cars that managed to find a spot in the park is %ld.\n",cur_parked_cars);
	printf("The maximum number of cars to occupy the park is %ld.\n",cur_max_cars);

	pthread_exit(NULL);
}
