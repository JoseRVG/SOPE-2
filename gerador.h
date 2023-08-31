#ifndef SOPE_P2_GERADOR_H_
#define SOPE_P2_GERADOR_H_

#include "comm_info.h"

//Global values (shared amongst threads)
struct car_thr_args
{
	struct park_request req;
	//used for time tracking
	clock_t start;
};

//Generation probabilities
#define PROB_0U	0.5
#define PROB_1U	0.3
#define PROB_2U	0.2

//Writting to generator log file
#define GEN_LOG	"gerador.log"
#define GEN_LOG_LINE_SIZE 150

#define GEN_FULL_OBSERV		"cheio!"
#define GEN_ENTRY_OBSERV	"entrada"
#define GEN_EXIT_OBSERV		"saida"
#define GEN_CLOSED_OBSERV	"encerrado"
#define MAX_GEN_OBSERV_SIZE 20

#endif /* SOPE_P2_GERADOR_H_ */
