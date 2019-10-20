all: parse.o request_handler.o main.o
	gcc -o server main.o parse.o request_handler.o

main.o:
	gcc -c main.c

request_handler.o: parse.o
	gcc -c request_handler.c 

parse.o: parse.c
	gcc -c parse.c

clean:
	rm *.o server