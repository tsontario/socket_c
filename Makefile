FLAGS = -std=gnu99

all: myServer.o request_handler.o parse.o
	gcc $(FLAGS) -o myServer myServer.o parse.o request_handler.o

myServer.o: myServer.c request_handler.h
	gcc $(FLAGS) -c myServer.c

request_handler.o: request_handler.c parse.h request_handler.h
	gcc $(FLAGS) -c request_handler.c 

parse.o: parse.c parse.h
	gcc $(FLAGS) -c parse.c

clean:
	rm *.o myServer