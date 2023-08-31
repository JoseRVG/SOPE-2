#ifndef SOPE_P2_PARQUE_H_
#define SOPE_P2_PARQUE_H_

#include "comm_info.h"

struct usher_thr_args
{
	struct park_request req;
	//used for time tracking
	clock_t start;
};

struct gate_thr_args
{
	char gate;
	//used for time tracking
	clock_t start;
};

//Writting to park log file
#define PARK_LOG	"parque.log"
#define PARK_LOG_LINE_SIZE 100

#define PARK_FULL_OBSERV	"cheio"
#define PARK_ENTRY_OBSERV	"estacionamento"
#define PARK_EXIT_OBSERV	"saida"
#define PARK_CLOSED_OBSERV	"encerrado"
#define MAX_PARK_OBSERV_SIZE 20


#endif /* SOPE_P2_PARQUE_H_ */
