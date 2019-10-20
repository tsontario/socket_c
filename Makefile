all: main.o socket.o
	gcc -o socket main.o socket.o

main.o: main.c socket.h
	gcc -c main.c

socket.o: socket.c socket.h
	gcc -c socket.c
