FLAGS = -std=gnu99

all: myServer.o request_handler.o parse.o
	gcc $(FLAGS) -o myServer myServer.o parse.o request_handler.o

windows: myServerWINDOWS.o request_handler.o parse.o
	gcc $(FLAGS) -o myServerWINDOWS myServerWINDOWS.o parse.o request_handler.o

myServerWINDOWS.o: myServerWINDOWS.c request_handler.h
	gcc $(FLAGS) -c myServerWINDOWS.c

myServer.o: myServer.c request_handler.h
	gcc $(FLAGS) -c myServer.c

request_handler.o: request_handler.c parse.h request_handler.h
	gcc $(FLAGS) -c request_handler.c 

parse.o: parse.c parse.h
	gcc $(FLAGS) -c parse.c

clean:
	rm *.o myServer