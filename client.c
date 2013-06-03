#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define FALSE 0
#define TRUE 1
#define CHATLOGSIZE 1048576
#define BUFFERSIZE 256

int sockfd;
int port;

pthread_t writer;
pthread_t reader;
pthread_attr_t pthread_custom_attr;

char chatLog[CHATLOGSIZE];
char typingBuffer[BUFFERSIZE];

pthread_mutex_t mutexRefresh;
pthread_mutex_t mutexLogout;

int online;

void processInput(int argc, char const *argv[]);
void initSocket(int port);
void login();
void* writeMessage(void* args);
void* readMessage(void* args);
void refresh();

int main(int argc, char const *argv[])
{
	processInput(argc, argv);

	pthread_mutex_init(&mutexRefresh,NULL);
	pthread_mutex_init(&mutexLogout,NULL);

	sprintf(chatLog,"");

	login();

	pthread_create(&writer, &pthread_custom_attr, writeMessage, (void*)0);
	pthread_create(&reader, &pthread_custom_attr, readMessage, (void*)0);

	pthread_join(writer, NULL);
	pthread_join(reader, NULL);

	return 0;
}

void processInput(int argc, char const *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr,"usage: %s port\n", argv[0]);
		exit(0);
	}

	port = 4000 + atoi(argv[1]);

	initSocket(port);
}

void initSocket(int port)
{
	struct sockaddr_in server_address;
	struct hostent *server;

	server = gethostbyname("0");

	if(server == NULL)
	{
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(sockfd == -1)
	{
		fprintf(stderr,"ERROR opening socket\n");
		exit(0);
	}

	server_address.sin_family = AF_INET;     
	server_address.sin_port = htons(port);    
	server_address.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(server_address.sin_zero), 8);

	int could_connect = connect(sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) >= 0;

	if(!could_connect)
	{
		fprintf(stderr,"ERROR connecting\n");
		exit(0);
	}
}

void login()
{
	char buffer[BUFFERSIZE];
	int n;

	fprintf(stderr,"Enter your username: ");
	bzero(buffer, BUFFERSIZE);
	fgets(buffer, BUFFERSIZE, stdin);

	buffer[strlen(buffer) -1] = '\0';

	/* write in the socket */
	n = write(sockfd, buffer, strlen(buffer));
	if(n < 0)
	{
		fprintf(stderr,"ERROR writing to socket\n");
		exit(0);
	}

	online = TRUE;
}

void* writeMessage(void* args)
{
	int n;

	while(online)
	{
		refresh();

    	bzero(typingBuffer, BUFFERSIZE);
    	fgets(typingBuffer, BUFFERSIZE, stdin);

    	typingBuffer[strlen(typingBuffer) -1] = '\0';
    	
		/* write in the socket */
		n = write(sockfd, typingBuffer, strlen(typingBuffer));
    	if (n < 0)
    	{
			fprintf(stderr,"ERROR writing to socket\n");
			exit(0);
		}

		bzero(typingBuffer, BUFFERSIZE);
	}

	exit(0);
}

void* readMessage(void* args)
{
	char buffer[BUFFERSIZE];
	int n;

	while(online)
	{
    	bzero(buffer,BUFFERSIZE);
		
		/* read from the socket */
    	n = read(sockfd, buffer, BUFFERSIZE);
    	if (n < 0)
    	{
			fprintf(stderr,"ERROR reading from socket\n");
			exit(0);
		}

		if(strcmp(buffer,"KILL") == 0)
		{
			pthread_mutex_lock(&mutexLogout);

			close(sockfd);

			online = FALSE;
			fprintf(stderr,"You are now offline!\n");

			exit(0);

			pthread_mutex_lock(&mutexLogout);

		}
		else
		{
			strcat(chatLog,buffer);
			strcat(chatLog,typingBuffer);

			refresh();
		}
	}

	exit(0);
}

void refresh()
{
	pthread_mutex_lock(&mutexRefresh);

	system("clear");
		
    fprintf(stderr,"%s\nType your messages here: ",chatLog);

    pthread_mutex_unlock(&mutexRefresh);
}