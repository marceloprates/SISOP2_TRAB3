
all: main client

main: main.c
	gcc -pthread -o main main.c

client: client.c
	gcc -pthread -o client client.c

clean:
	rm *.o main client