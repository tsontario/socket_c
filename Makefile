all: main.o request_handler.o
	gcc -o server main.o request_handler.o

main.o: main.c request_handler.h
	gcc -c main.c

socket.o: request_handler.c request_handler.h
	gcc -c request_handler.c

clean:
	rm *.o server