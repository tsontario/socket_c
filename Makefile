FLAGS = -std=gnu99

all: main.o request_handler.o parse.o
	gcc $(FLAGS) -o server main.o parse.o request_handler.o

main.o: main.c request_handler.h
	gcc $(FLAGS) -c main.c

request_handler.o: request_handler.c parse.h
	gcc $(FLAGS) -c request_handler.c 

parse.o: parse.c parse.h
	gcc $(FLAGS) -c parse.c

clean:
	rm *.o server