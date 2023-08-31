#ifndef SOPE_P2_COMM_INFO_H_
#define SOPE_P2_COMM_INFO_H_

#include <sys/times.h> //clock_t
#include <pthread.h> //sizeof(pthread_t)
#include <string.h> //strlen

//Park gates
#define NORTH_GATE 'N'
#define SOUTH_GATE 'S'
#define WEST_GATE 'O'
#define EAST_GATE 'E'
#define CLOSE_PARK 'C' //special command to close the park (so we don't need to create a new variable...)

#define NUM_GATES	4
#define GATES	{NORTH_GATE, SOUTH_GATE, WEST_GATE, EAST_GATE}

#define NORTH_GATE_FIFO "/tmp/fifoN"
#define SOUTH_GATE_FIFO "/tmp/fifoS"
#define WEST_GATE_FIFO "/tmp/fifoO"
#define EAST_GATE_FIFO "/tmp/fifoE"
#define GATE_FIFOS	{NORTH_GATE_FIFO, SOUTH_GATE_FIFO, WEST_GATE_FIFO, EAST_GATE_FIFO}
#define GATE_FIFO_NAME_LEN 20

#define PERMISSIONS 0666

//Park request
#define PRIVATE_FIFO_NAME_LEN 50

struct park_request
{
	char gate;
	clock_t park_time;
	unsigned int car_id;
	char fifo_name[PRIVATE_FIFO_NAME_LEN];
};

//Park answer
#define FULL_RESP	'f'
#define ENTRY_RESP	'e'
#define EXIT_RESP	's'
#define CLOSED_RESP 'c'

struct park_answer
{
	char response;
};

//Semaphore for writing to the gate FIFOS
#define NORTH_GATE_SEM "/gate_semN"
#define SOUTH_GATE_SEM "/gate_semS"
#define WEST_GATE_SEM "/gate_semO"
#define EAST_GATE_SEM "/gate_semE"
#define GATE_SEMS {NORTH_GATE_SEM, SOUTH_GATE_SEM, WEST_GATE_SEM, EAST_GATE_SEM}
#define GATE_SEM_NAME_LEN 20

//For writing to the log files
#define UNDEFINED_VALUE	"?"

#endif /* SOPE_P2_COMM_INFO_H_ */
