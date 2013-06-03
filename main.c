#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define FALSE 0
#define TRUE 1
#define MESSAGESIZE 256
#define USERNAMESIZE 256

typedef struct
{
	int in;

} param;

typedef struct
{
	int online;
	int id;
	int port;
	int socket;
	char* username;
	pthread_t thread;

} Client;

Client* clients;
int roomSize;

pthread_attr_t pthread_custom_attr;

void processInput(int argc, char const *argv[]);
void* worker(void* args);
void initSocket(int threadIndex);
void readUsername(int threadIndex);
void readMessages(int threadIndex);
void sendToAllClients(char* message);
void getTime(char* buf);
void toLower(char* destination, char* source);

int main(int argc, char const *argv[])
{
	processInput(argc,argv);

	return 0;
}

void processInput(int argc, char const *argv[])
{
	int i;

	if(argc < 2)
	{
		fprintf(stderr,"usage: %s #clients\n", argv[0]);
		exit(0);
	}

	roomSize = atoi(argv[1]);

	// allocating memory for clients array
	clients = (Client*)calloc(roomSize,sizeof(Client));

	// initializing clients
	for(i = 0; i < roomSize; i++)
	{
		clients[i].online = FALSE;
		clients[i].id = i;
		clients[i].port = 4000 + i;
		clients[i].username = (char*)calloc(USERNAMESIZE,sizeof(char));		
	}

	// creating all clients' threads
	for(i = 0; i < roomSize; i++)
	{
		pthread_create(&(clients[i].thread), &pthread_custom_attr, worker, &(clients[i].id));
	}

	// waiting for all clients' threads
	for(i = 0; i < roomSize; i++)
	{
		pthread_join(clients[i].thread, NULL);
	}
}

void* worker(void* args)
{
	param* p = (param*)args;
	int clientId = p->in;

	fprintf(stderr,"DEBUG: this is thread number %d\n",clientId);

	initSocket(clientId);

	readUsername(clientId);

	readMessages(clientId);
}

void initSocket(int clientId)
{
	int sockfd, newsockfd;
	socklen_t clientLength;
	struct sockaddr_in server_address, client_address;

	// opening socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(sockfd == -1)
	{
		fprintf(stderr, "ERROR opening socket\n");
		exit(0);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(clients[clientId].port);
	server_address.sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_address.sin_zero), 8);

	// binding
	int could_bind = bind(sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) >= 0;

	if(!could_bind)
	{
		fprintf(stderr, "ERROR on binding\n");
		exit(0);
	}

	// listening
	listen(sockfd, 5);

	clientLength = sizeof(struct sockaddr_in);

	// accepting
	newsockfd = accept(sockfd, (struct sockaddr*) &client_address, &clientLength);

	if(newsockfd == -1)
	{
		fprintf(stderr,"ERROR on accept\n");
		exit(0);
	}

	clients[clientId].socket = newsockfd;
	clients[clientId].online = TRUE;

	close(sockfd);
}

void readUsername(int clientId)
{
	int n;
	char buffer[USERNAMESIZE];

	bzero(buffer, USERNAMESIZE);

	n = read(clients[clientId].socket, buffer, USERNAMESIZE);

	if(n < 0)
	{
		fprintf(stderr,"ERROR reading from socket\n");
		exit(0);
	}

	sprintf(clients[clientId].username,"%s",buffer);

	char timeString[80];

	getTime(timeString);

	fprintf(stderr,"%s	Client in port %d's username: %s\n", timeString, clients[clientId].id, clients[clientId].username);
}

void readMessages(int clientId)
{
	int n;
	char message[MESSAGESIZE];

	while(1)
	{
		char timeString[80];
		getTime(timeString);

		bzero(message, MESSAGESIZE);
	
		n = read(clients[clientId].socket, message, MESSAGESIZE);
	
		if(n < 0)
		{
			fprintf(stderr,"ERROR reading from socket\n");
			exit(0);
		}

		char* lowercaseMessage = (char*)calloc(strlen(message),sizeof(char));
		toLower(lowercaseMessage,message);

		if(strcmp(lowercaseMessage,"logout") == 0)
		{
			clients[clientId].online = FALSE;

			char logoutMessage[MESSAGESIZE];
			sprintf(logoutMessage,"%s	%s LOGGED OUT\n", timeString, clients[clientId].username);
			sendToAllClients(logoutMessage);

			n = write(clients[clientId].socket, "KILL", strlen("KILL"));

			if(n < 0)
			{
				fprintf(stderr,"ERROR writing to socket");
				exit(0);
			}

			close(clients[clientId].socket);

			return;
		}
		else
		{
			char saysMessage[MESSAGESIZE];

			sprintf(saysMessage, "%s	%s says: %s\n", timeString, clients[clientId].username, message);
			sendToAllClients(saysMessage);
		}
	}
}

void sendToAllClients(char* message)
{
	int i, n;

	for(i = 0; i < roomSize; i++)
	{
		Client client = clients[i];

		if(client.online)
		{
			n = write(clients[i].socket, message, strlen(message));

			if(n < 0)
			{
				fprintf(stderr,"ERROR writing to socket");
				exit(0);
			}
		}
	}
}

void getTime(char* buf)
{
	time_t     now = time(0);
    struct tm  tstruct;
    tstruct = *localtime(&now);

    sprintf(buf,"%2d:%2d:%2d", tstruct.tm_hour, tstruct.tm_min, tstruct.tm_sec);
}

void toLower(char* destination, char* source)
{
	int i;

	for(i = 0; i < strlen(source); i++)
	{
		destination[i] = tolower(source[i]);
	}
}