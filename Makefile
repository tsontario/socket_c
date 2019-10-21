FLAGS = -std=gnu99

all: main.o request_handler.o parse.o
	gcc $(FLAGS) -o server main.o parse.o request_handler.o

main.o: request_handler.o parse.o
	gcc $(FLAGS) -c main.c

request_handler.o: parse.o
	gcc $(FLAGS) -c request_handler.c 

parse.o: parse.c
	gcc $(FLAGS) -c parse.c

clean:
	rm *.o server