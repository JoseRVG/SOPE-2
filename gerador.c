#include <stdio.h> //printf
#include <sys/types.h> //open
#include <sys/stat.h> //open
#include <fcntl.h> //open
#include <signal.h> //pthread_sigmask
#include <stdlib.h> //malloc, free, strtol
#include <unistd.h> //read, write, unlink, sysconf
#include <errno.h> //errno
#include <time.h> //time (to initialize seedp)
#include <semaphore.h> //sem_open

#include "gerador.h"

void writeToGenLog(clock_t cur_ticks, struct park_request req, struct park_answer ans, clock_t t_vida)
{
	//Step 1: Open the log file
	int log_fd;
	if((log_fd = open(GEN_LOG,O_WRONLY|O_APPEND)) == -1)
	{
		perror("open of generator log file");
		return;
	}

	//Step 2: Define the line to write to the log file
	char line[GEN_LOG_LINE_SIZE];
	switch(ans.response)
	{
	case ENTRY_RESP:
		sprintf(line,"\n%15u ; %15u ; %15c ; %15u ; %15u ; %15s",
				(unsigned int)(cur_ticks),req.car_id,req.gate,
				(unsigned int)req.park_time,(unsigned int)t_vida,
				GEN_ENTRY_OBSERV);
		break;
	case EXIT_RESP:
		sprintf(line,"\n%15u ; %15u ; %15c ; %15u ; %15u ; %15s",
				(unsigned int)(cur_ticks),req.car_id,req.gate,
				(unsigned int)req.park_time,(unsigned int)t_vida,
				GEN_EXIT_OBSERV);
		break;
	case FULL_RESP:
		sprintf(line,"\n%15u ; %15u ; %15c ; %15u ; %15u ; %15s",
				(unsigned int)(cur_ticks),req.car_id,req.gate,
				(unsigned int)req.park_time,(unsigned int)t_vida,
				GEN_FULL_OBSERV);
		break;
	case CLOSED_RESP:
		sprintf(line,"\n%15u ; %15u ; %15c ; %15u ; %15u ; %15s",
				(unsigned int)(cur_ticks),req.car_id,req.gate,
				(unsigned int)req.park_time,(unsigned int)t_vida,
				GEN_CLOSED_OBSERV);
		break;
	}

	//Step 3: Write down on the generator log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write to the generator log file");
		close(log_fd);
		return;
	}

	//Step 4: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close of the generator log file");
	}

	return;
}

void *car_thr(void *arg)
{
	clock_t start_life = times(NULL);

	//Step 1: Define the thread as detached
	if(pthread_detach(pthread_self()) != 0)
	{
		printf("pthread_detach failed\n");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 2: Retrieve thread's arguments
	struct car_thr_args *args = (struct car_thr_args *)arg;
	struct park_request *req = &(args->req);

	//Step 3: Create private FIFO to receive data from the park
	sprintf(req->fifo_name,"/tmp/%lu",(unsigned long)pthread_self());
	if(mkfifo(req->fifo_name,PERMISSIONS) == -1)
	{
		perror("mkfifo");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 4: Obtain the xmit FIFO and semaphore names
	char xmit_fifo_name[GATE_FIFO_NAME_LEN];
	char sem_name[GATE_SEM_NAME_LEN];
	switch(req->gate)
	{
	case NORTH_GATE:
		strcpy(xmit_fifo_name,NORTH_GATE_FIFO);
		strcpy(sem_name,NORTH_GATE_SEM);
		break;
	case SOUTH_GATE:
		strcpy(xmit_fifo_name,SOUTH_GATE_FIFO);
		strcpy(sem_name,SOUTH_GATE_SEM);
		break;
	case WEST_GATE:
		strcpy(xmit_fifo_name,WEST_GATE_FIFO);
		strcpy(sem_name,WEST_GATE_SEM);
		break;
	case EAST_GATE:
		strcpy(xmit_fifo_name,EAST_GATE_FIFO);
		strcpy(sem_name,EAST_GATE_SEM);
		break;
	}

	//Step 5: Open the semaphore
	sem_t *sem;
	sem = sem_open(sem_name,0);
	if(sem == SEM_FAILED)
	{
		/*The  O_CREAT  flag  was  not specified in oflag and no semaphore
		  with this name exists;  or,  O_CREAT  was  specified,  but  name
		  wasn't well formed. (man sem_open for ENOENT)*/
		//If sem_open returns with this error, it means the park is not open.
		if(errno != ENOENT)
		{
			perror("sem_open failed");
		}
		else
		{
			clock_t end = times(NULL);
			clock_t cur_ticks = (end - args->start);
			clock_t t_vida = (end - start_life);
			struct park_answer ans;
			ans.response = CLOSED_RESP;
			writeToGenLog(cur_ticks,*req,ans,t_vida);
		}

		if(unlink(req->fifo_name) == -1)
		{
			perror("unlink");
		}

		free(arg);
		pthread_exit(NULL);
	}

	//Step 6: Open the xmit FIFO
	int fd_xmit = open(xmit_fifo_name,O_WRONLY|O_NONBLOCK);
	if(fd_xmit == -1)
	{
		perror("open of xmit FIFO");

		clock_t end = times(NULL);
		clock_t cur_ticks = (end - args->start);
		clock_t t_vida = (end - start_life);
		struct park_answer ans;
		ans.response = CLOSED_RESP;
		writeToGenLog(cur_ticks,*req,ans,t_vida);

		if(sem_close(sem) != 0)
		{
			perror("sem_close");
			unlink(req->fifo_name);
			free(arg);
			pthread_exit(NULL);
		}

		if(unlink(req->fifo_name) == -1)
		{
			perror("unlink");
			free(arg);
			pthread_exit(NULL);
		}

		free(arg);
		pthread_exit(NULL);
	}

	//Step 7: Send park request
	int ret = sem_wait(sem);
	if(ret == -1)
	{
		perror("sem_wait");
	}
	//////// Start of critical section for sending a park request ////////
	ssize_t write_ret = write(fd_xmit,req,sizeof(struct park_request));
	if(write_ret == -1)
	{
		perror("write");
	}
	//////// End of critical section for sending a park request ////////
	ret = sem_post(sem);
	if(ret != 0)
	{
		perror("sem_post");
	}
	if(write_ret == -1) //this wouldn't need to be inside the critical section (but perror needs to so that errno isn't changed)
	{
		sem_close(sem);

		clock_t end = times(NULL);
		clock_t cur_ticks = (end - args->start);
		clock_t t_vida = (end - start_life);
		struct park_answer ans;
		ans.response = CLOSED_RESP;
		writeToGenLog(cur_ticks,*req,ans,t_vida);

		close(fd_xmit);
		unlink(req->fifo_name);
		free(arg);
		pthread_exit(NULL);
	}

	//From here now on, we can assume that the server (park) has received the request, and that thid thread will receive a response

	//Step 8: Close the semaphore
	if(sem_close(sem) != 0)
	{
		perror("sem_close");
	}

	//Step 9: Close xmit FIFO
	if(close(fd_xmit) == -1)
	{
		perror("close");
	}

	//Step 10: Open rcvr FIFO
	int fd_rcvr = open(req->fifo_name,O_RDONLY);
	if(fd_rcvr == -1)
	{
		perror("open");
	}

	//Step 11: Read park's answer
	struct park_answer ans;
	if(read(fd_rcvr,&ans,sizeof(struct park_answer)) == -1)
	{
		perror("read");
	}

	if(ans.response == FULL_RESP)
	{
		//Step 12a: Write to the log file
		clock_t end = times(NULL);
		clock_t cur_ticks = (end - args->start);
		clock_t t_vida = (end - start_life);
		writeToGenLog(cur_ticks,*req,ans,t_vida);
	}
	else
	{
		//Step 12A: Write to the log file
		clock_t end = times(NULL);
		clock_t cur_ticks = (end - args->start);
		clock_t t_vida = (end - start_life);
		writeToGenLog(cur_ticks,*req,ans,t_vida);

		//Step 13A: Read exit response from the rcvr FIFO
		if(read(fd_rcvr,&ans,sizeof(struct park_answer)) == -1)
		{
			perror("read");
		}

		//Step 14A: Write to the log file
		end = times(NULL);
		cur_ticks = (end - args->start);
		t_vida = (end - start_life);
		writeToGenLog(cur_ticks,*req,ans,t_vida);
	}

	//Step 15: Close rcvr FIFO
	if(close(fd_rcvr) == -1)
	{
		perror("close");
		unlink(req->fifo_name);
		free(arg);
		pthread_exit(NULL);
	}

	//Step 16: Destroy rcvr FIFO
	if(unlink(req->fifo_name) == -1)
	{
		perror("unlink");
		free(arg);
		pthread_exit(NULL);
	}

	//Step 17: Free the argument memory
	free(arg);

	pthread_exit(NULL);
}

clock_t generate_waitTime(unsigned int *seedp, clock_t u_rel)
{
	int rd = rand_r(seedp);
	if((rd % 100) < PROB_0U*100)
	{
		return 0*u_rel;
	}
	else if((rd % 100) < (PROB_0U + PROB_1U)*100)
	{
		return 1*u_rel;
	}
	else
	{
		return 2*u_rel;
	}
}

void generate_car(struct park_request *req, unsigned int *seedp, unsigned int *n_generated, clock_t u_rel)
{
	//Step 1: Define the vehicle's id
	(*n_generated)++;
	req->car_id = (*n_generated);

	//Step 2: Define the park gate to visit
	int rd = rand_r(seedp);
	switch(rd % 4)
	{
	case 0:
		req->gate = NORTH_GATE;
		break;
	case 1:
		req->gate = SOUTH_GATE;
		break;
	case 2:
		req->gate = WEST_GATE;
		break;
	default: //3
		req->gate = EAST_GATE;
		break;
	}

	//Step 3: Define the park time
	rd = rand_r(seedp);
	req->park_time = u_rel*((rd % 10) + 1);
}

int main(int argc, char *argv[])
{
	//Step 1: Check command line arguments
	if(argc != 3)
	{
		printf("Uso: gerador <T_GERACAO> <U_RELOGIO>\n");
		return 1;
	}

	errno = 0;
	/*"the calling program should set errno to 0 before the call, and then deter‐
    mine if an error occurred by checking whether errno has a nonzero value
    after the call." (man strtol)*/
	const long int t_ger_secs = strtol(argv[1],NULL,10);
	if(errno != 0)
	{
		perror("strtol for T_GERACAO");
		pthread_exit(NULL);
	}
	if(t_ger_secs <= 0)
	{
		printf("T_GERACAO must contain a positive value\n");
		pthread_exit(NULL);
	}

	errno = 0;
	const clock_t u_rel = (clock_t)strtol(argv[2],NULL,10);
	if(errno != 0)
	{
		perror("strtol for U_RELOGIO");
		pthread_exit(NULL);
	}
	if(u_rel <= 0)
	{
		printf("U_RELOGIO must contain a positive value\n");
		pthread_exit(NULL);
	}


	//Step 2: Initialize the several variables
	const long int ticks_per_sec = sysconf(_SC_CLK_TCK);
	if(ticks_per_sec == -1)
	{
		perror("sysconf");
		pthread_exit(NULL);
	}
	const double micros_per_tick = 1000000/((double)ticks_per_sec);

	const clock_t t_ger_ticks = t_ger_secs * ticks_per_sec;
	clock_t start = times(NULL);

	unsigned int seedp; //used in rand_r
	seedp = time(NULL); //initialize random number generation

	unsigned int n_generated = 0; //used to generate the next car id

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

	//Step 4: Create the log file
	int log_fd;
	if((log_fd = open(GEN_LOG,O_WRONLY|O_CREAT|O_TRUNC,PERMISSIONS)) == -1)
	{
		perror("open of generator logfile (first time)");
		pthread_exit(NULL);
	}

	//Step 5: Write the log file's header
	char line[GEN_LOG_LINE_SIZE];
	sprintf(line,"%15s ; %15s ; %15s ; %15s ; %15s ; %15s",
			"t(ticks)","id_viat","destin",
			"t_estacion","t_vida",
			"observ");

	//Step 6: Write down on the generator log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
		pthread_exit(NULL);
	}

	//Step 7: Write a log entry to note down the absolute number of clock ticks at which the process started
	sprintf(line,"\n%15u ; %15s ; %15s ; %15s ; %15s ; %15s",
			(unsigned int)(start),UNDEFINED_VALUE,UNDEFINED_VALUE,
			UNDEFINED_VALUE,UNDEFINED_VALUE,
			"abertura");

	//Step 8: Write down on the generator log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
		pthread_exit(NULL);
	}

	//Step 9: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close");
		pthread_exit(NULL);
	}

	clock_t end = times(NULL);
	clock_t cur_ticks = (end - start);


	//Step 10: Generate vehicles until the time is up
	pthread_t tids[100];
	long int total_cars = 0;
	while(cur_ticks < t_ger_ticks)
	{
		struct car_thr_args *args = (struct car_thr_args *)malloc(sizeof(struct car_thr_args));
		if(args == NULL)
		{
			printf("malloc of a car_thr_args struct failed\n");
			pthread_exit(NULL);
		}

		generate_car(&(args->req),&seedp,&n_generated,u_rel);
		args->start = start;

		if(pthread_create(&tids[n_generated % 100],NULL,car_thr,args) != 0)
		{
			printf("pthread_create failed\n");
			pthread_exit(NULL);
		}

		total_cars++;

		clock_t ticks_to_next_car = generate_waitTime(&seedp,u_rel);
		if(usleep(ticks_to_next_car*micros_per_tick) == -1)
		{
			perror("usleep");
			pthread_exit(NULL);
		}

		end = times(NULL);
		cur_ticks = (end - start);
	}

	//Step 11: Open the log file
	if((log_fd = open(GEN_LOG,O_WRONLY|O_APPEND)) == -1)
	{
		perror("open of generator log file");
	}

	//Step 12: Write a log entry to note down the absolute number of clock ticks at which the process ended
	end = times(NULL);
	sprintf(line,"\n%15u ; %15s ; %15s ; %15s ; %15s ; %15s",
			(unsigned int)(end),UNDEFINED_VALUE,UNDEFINED_VALUE,
			UNDEFINED_VALUE,UNDEFINED_VALUE,
			"fecho");

	//Step 13: Write down on the generator log file
	if(write(log_fd,line,strlen(line)) == -1)
	{
		perror("write");
	}

	//Step 14: Close the log file
	if(close(log_fd) == -1)
	{
		perror("close");
	}

	//Step 15: Print out some global statistics
	printf("Generator process has successfully ended.\n");
	printf("The total number of cars that were created is %ld.\n",total_cars);

	pthread_exit(NULL);
}
