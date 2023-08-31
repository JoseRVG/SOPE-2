all: gerador parque

gerador: gerador.c gerador.h comm_info.h bin
	gcc -o bin/gerador gerador.c -Wall -pthread

parque: parque.c parque.h comm_info.h bin
	gcc -o bin/parque parque.c -Wall -pthread
	
bin:
	mkdir bin
	
clean: 
	rm -f bin/gerador bin/parque